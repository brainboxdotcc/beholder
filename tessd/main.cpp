/************************************************************************************
 * 
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023,2026 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0
 *
 ************************************************************************************/
#include <dpp/dpp.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <beholder/beholder.h>
#include <beholder/proc/json_frame.h>
#include <beholder/proc/spawn.h>
#include <beholder/tessd.h>
#include "3rdparty/httplib.h"
#include <CxxUrl/url.hpp>
#include <cctype>
#include <csignal>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <fmt/core.h>
#include <vector>

/**
 * @brief Tesseract Daemon
 *
 * This is a separate standalone program which scans an image whos data is given via stdin,
 * and outputs the text via stdout once EOF is received on stdin.
 *
 * This was originally done within the main bot process as part of ocr.cpp, and although it was
 * a fair bit faster, libtesseract has some huge memory leaks, which happen when you call
 * tesseract::TessBaseAPI::GetUTF8Text(). These are internal leaks, which I do not have the
 * inclination or technical knowledge in image processing internals to fix. Checking on their
 * issue tracker on github shows 400 issues, dating back as far as 2017 which are still open.
 * This means that if i report the leak, it won't get fixed any time in a reasonable timeframe.
 * As the main program which uses tesseract is a short lived program like this, it is likely
 * the never noticed the issue.
 *
 * By isolating tesseract in its own program like this, we ensure that Linux can free up the
 * memory leak for us. We can also pre-launch copies of this program, each waiting for stdin
 * in a similar way to how fastcgi works. This would improve performance if needed.
 */

constexpr uint64_t one_gigabyte = 1073741824;
constexpr uint64_t max_pixels = 33554432;

struct scan_result {
	std::string scanner;
	std::string scanner_name;
	bool enabled{false};
	bool blocked{false};
	std::string text;
	double trigger{0.0};
	double threshold{0.0};
	dpp::json raw = dpp::json::object();
	dpp::json cache = nullptr;
};

Pix* rgba_to_pix(const unsigned char* pixels, int width, int height)
{
	if (!pixels || width <= 0 || height <= 0) {
		return nullptr;
	}

	Pix* image = pixCreate(width, height, 32);

	if (!image) {
		return nullptr;
	}

	l_uint32* destination = pixGetData(image);
	const int words_per_line = pixGetWpl(image);

	for (int y = 0; y < height; ++y) {
		l_uint32* destination_line = destination + (y * words_per_line);
		const unsigned char* source_line = pixels + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4);

		for (int x = 0; x < width; ++x) {
			const unsigned char* source = source_line + (x * 4);
			composeRGBPixel(source[0], source[1], source[2], &destination_line[x]);
		}
	}

	return image;
}

void tessd_timeout(int sig)
{
	tessd::status(tessd::exit_code::timeout);
}

void set_limit(int type, uint64_t max)
{
	rlimit64 r{};

	if (getrlimit64(type, &r) != -1) {
		if (r.rlim_cur > max || r.rlim_max > max || r.rlim_max == 0) {
			r.rlim_cur = max;
			r.rlim_max = max;
			setrlimit64(type, &r);
		}
	}
}

void write_error(const std::string& stage, const std::string& error)
{
	proc::write_frame({
		{"stage", stage},
		{"status", "error"},
		{"error", error}
	});
}

void write_error(const std::string& stage, const std::string& error, const std::string& detail)
{
	proc::write_frame({
		{"stage", stage},
		{"status", "error"},
		{"error", error},
		{"detail", detail}
	  });
}

std::string build_path_with_query(const Url& url)
{
	std::string path = url.path();

	if (!url.query().empty()) {
		std::stringstream query;
		bool first = true;

		for (const auto& p : url.query()) {
			if (!first) {
				query << "&";
			}

			query << p.key() << "=" << p.val();
			first = false;
		}

		path += "?" + query.str();
	}

	return path;
}

bool validate_image_dimensions(const std::string& file_content)
{
	Pix* image = pixReadMem(reinterpret_cast<const l_uint8*>(file_content.data()), file_content.size());

	if (!image) {
		return false;
	}

	const uint64_t pixels = static_cast<uint64_t>(image->w) * static_cast<uint64_t>(image->h);
	pixDestroy(&image);

	return pixels <= max_pixels;
}

bool fetch_image(const std::string& url, std::string& file_content)
{
	Url parsed(url);
	const std::string host = parsed.scheme() + "://" + parsed.host();
	const std::string path = build_path_with_query(parsed);

	httplib::Client cli(host.c_str());
	cli.enable_server_certificate_verification(false);

	if (host != "https://cdn.discordapp.com") {
		cli.set_proxy("127.0.0.1", 9080);
	}

	auto res = cli.Get(path);

	if (!res) {
		write_error("fetch", "download_failed", httplib::to_string(res.error()));
		return false;
	}

	if (res->status >= 400) {
		proc::write_frame({
			{"stage", "fetch"},
			{"status", "error"},
			{"error", "http_error"},
			{"http_status", res->status}
		});
		return false;
	}

	if (res->body.size() > max_size) {
		proc::write_frame({
			{"stage", "fetch"},
			{"status", "error"},
			{"error", "image_too_large"},
			{"size", res->body.size()}
		});
		return false;
	}

	if (!validate_image_dimensions(res->body)) {
		write_error("fetch", "invalid_image");
		return false;
	}

	file_content = std::move(res->body);
	return true;
}

bool is_animated_gif(const std::string& file_content)
{
	if (file_content.length() < 6) {
		return false;
	}

	/* Animated gifs require a control structure only available in GIF89a, GIF87a is fine and anything that is
	 * neither is not a GIF file.
	 * By the way, it's pronounced GIF, as in GOLF, not JIF, as in JUMP! 🤣
	 */
	const auto* filebits = reinterpret_cast<const uint8_t*>(file_content.data());
	if (filebits[0] != 'G' || filebits[1] != 'I' || filebits[2] != 'F' || filebits[3] != '8' || filebits[4] != '9' || filebits[5] != 'a') {
		return false;
	}

	for (size_t pos = 0; pos + 3 < file_content.length(); ++pos) {
		/* If control structure is found, sequence 21 F9 04, it is likely animated
		 * This is a much faster, more lightweight check than using a GIF library.
		 */
		if (filebits[pos] == 0x21 && filebits[pos + 1] == 0xF9 && filebits[pos + 2] == 0x04) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Given an image file, check if it is a gif, and if it is animated.
 * If it is, flatten it by extracting just the first frame using imagemagick.
 *
 * @param bot Reference to D++ cluster
 * @param attach message attachment
 * @param file_content file content
 * @return std::string new file content
 */
std::string flatten_gif(const std::string& filename, const std::string& file_content)
{
	if (!is_animated_gif(file_content)) {
		return file_content;
	}

	const char* const argv[] = {"/usr/bin/convert", "-[0]", "png:-", nullptr};
	spawn convert(argv);
	convert.stdin.write(file_content.data(), file_content.length());
	convert.send_eof();

	std::ostringstream stream;
	stream << convert.stdout.rdbuf();

	if (convert.wait() != 0) {
		return file_content;
	}

	const std::string flattened = stream.str();

	if (flattened.empty()) {
		return file_content;
	}

	return flattened;
}

std::vector<std::string> json_string_array(const dpp::json& value)
{
	std::vector<std::string> out;

	if (!value.is_array()) {
		return out;
	}

	for (const auto& item : value) {
		if (item.is_string()) {
			out.emplace_back(item.get<std::string>());
		}
	}

	return out;
}

bool json_bool(const dpp::json& value, const std::string& key, bool fallback)
{
	if (!value.is_object() || !value.contains(key) || !value.at(key).is_boolean()) {
		return fallback;
	}

	return value.at(key).get<bool>();
}

static std::string run_tesseract_image_with_psm(Pix* image, tesseract::PageSegMode psm)
{
	/**
	 * RAII isn't a thing in tesseract land. We can't just initialise the object
	 * by `new`, we have to separately call an Init method. One of many
	 * anti-patterns.
	 */
	tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();

	if (api->Init(nullptr, "eng", tesseract::OcrEngineMode::OEM_DEFAULT)) {
		delete api;
		throw std::runtime_error("tesseract_init_failed");
	}

	api->SetPageSegMode(psm);
	api->SetImage(image);

	const char* output = api->GetUTF8Text();

	/**
	 * We have to call Clear to get rid of the data we loaded in SetImage.
	 * Destroying the api object isn't enough. RAII, whats that? herp derp.
	 */
	api->Clear();
	delete api;

	if (!output) {
		throw std::runtime_error("no_ocr_output");
	}

	/**
	 * We have to delete[] this content. Why couldn't it just return
	 * a std::string? i dont know...
	 */
	std::string text = output;
	delete[] output;

	return text;
}

std::string run_tesseract_image(Pix* image)
{
	std::string block_text = run_tesseract_image_with_psm(image, tesseract::PageSegMode::PSM_SINGLE_BLOCK);
	std::string sparse_text = run_tesseract_image_with_psm(image, tesseract::PageSegMode::PSM_SPARSE_TEXT);

	return block_text + "\n" + sparse_text;
}

bool has_text(const std::string& text)
{
	for (char c : text) {
		if (!std::isspace(static_cast<unsigned char>(c))) {
			return true;
		}
	}

	return false;
}

std::string run_tesseract_gif(const std::string& file_content, const std::vector<std::size_t>& frames)
{
	std::string text;

	decode_gif_frames(
		reinterpret_cast<const unsigned char*>(file_content.data()),
		file_content.size(),
		frames,
		[&text](std::size_t, const unsigned char* pixels, int width, int height) {
			const uint64_t pixel_count = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);

			if (pixel_count > max_pixels) {
				throw std::runtime_error("image_size");
			}

			Pix* image = rgba_to_pix(pixels, width, height);

			if (!image) {
				throw std::runtime_error("pix_create_failed");
			}

			std::string frame_text;

			try {
				frame_text = run_tesseract_image(image);
			} catch (...) {
				pixDestroy(&image);
				throw;
			}

			pixDestroy(&image);

			if (has_text(frame_text)) {
				if (!text.empty()) {
					text += "\n";
				}

				text += frame_text;
			}
		}
	);

	return text;
}

std::string run_tesseract(const std::string& file_content)
{
	/**
	 * We have to use leptonica to load images into tesseract.
	 * Leptonica is ugly and pretty undocumented. Here be hairy
	 * dragons.
	 */
	Pix* image = pixReadMem(reinterpret_cast<const l_uint8*>(file_content.data()), file_content.size());

	if (!image) {
		throw std::runtime_error("pix_read_mem_failed");
	}

	/**
	 * NOTE: The width, height and size attributes given here are only valid if the image was uploaded as
	 * an attachment. If the image we are processing came from a URL these can't be filled yet, and will
	 * be checked after we have downloaded the image. Bandwidth is cheap, so this doesnt matter too much,
	 * it's just the processing cost of running OCR on a massive image we would want to prevent.
	 */
	if (static_cast<uint64_t>(image->w) * static_cast<uint64_t>(image->h) > max_pixels) {
		pixDestroy(&image);
		throw std::runtime_error("image_size");
	}

	std::string text = run_tesseract_image(image);

	if (has_text(text)) {
		pixDestroy(&image);
		return text;
	}

	Pix* shaped = nullptr;

	if (image->d == 8) {
		shaped = pixClone(image);
	} else if (image->d == 32) {
		shaped = pixConvertRGBToGrayFast(image);
	} else {
		shaped = pixConvertTo8(image, false);
	}

	pixDestroy(&image);

	if (!shaped) {
		throw std::runtime_error("pix_shape_failed");
	}

	text = run_tesseract_image(shaped);
	pixDestroy(&shaped);

	return text;
}

scan_result scan_ocr(const dpp::json& command, const std::string& file_content, const std::vector<std::size_t>& frames)
{
	scan_result result;
	result.scanner = "ocr";
	result.scanner_name = "Text Recognition Rules";

	const std::vector<std::string> patterns = command.contains("ocr_patterns") ? json_string_array(command.at("ocr_patterns")) : std::vector<std::string>{};

	if (patterns.empty()) {
		result.text = "No match or not enabled";
		return result;
	}

	result.enabled = true;

	std::string ocr_text;

	if (command.contains("cache") && command.at("cache").contains("ocr") && command.at("cache").at("ocr").is_string()) {
		ocr_text = command.at("cache").at("ocr").get<std::string>();
	} else {
		ocr_text = frames.empty() ? run_tesseract(file_content) : run_tesseract_gif(file_content, frames);
		result.cache = ocr_text;
	}

	result.raw = {
		{"text", ocr_text}
	};

	std::vector<std::string> lines = dpp::utility::tokenize(ocr_text, "\n");

	for (const std::string& line : lines) {
		for (const std::string& pattern : patterns) {
			const std::string p = replace_string(pattern, "\r", "");

			if (line.empty() || p.empty()) {
				continue;
			}

			const std::string pattern_wild = "*" + p + "*";

			if (match(line.c_str(), pattern_wild.c_str())) {
				result.blocked = true;
				result.text = p;
				return result;
			}
		}
	}

	result.text = "No match";
	return result;
}

dpp::json run_basic_nsfw(const std::string& file_content)
{
	httplib::Client cli("http://localhost:6969");
	auto res = cli.Post("/", file_content, "application/octet-stream");

	if (!res) {
		throw std::runtime_error("NSFW API Error: " + httplib::to_string(res.error()));
	}

	if (res->status >= 400) {
		throw std::runtime_error("NSFW API HTTP status " + std::to_string(res->status));
	}

	dpp::json answer = dpp::json::parse(res->body);

	if (answer.contains("error")) {
		throw std::runtime_error("NSFW API Error: " + answer.at("error").get<std::string>());
	}

	return answer;
}

std::string pix_to_png(Pix* image)
{
	l_uint8* data = nullptr;
	size_t size = 0;

	if (!image || pixWriteMemPng(&data, &size, image, 0.0f) != 0 || !data || size == 0) {
		if (data) {
			lept_free(data);
		}

		throw std::runtime_error("pix_write_png_failed");
	}

	std::string content(reinterpret_cast<const char*>(data), size);
	lept_free(data);

	return content;
}

dpp::json run_basic_nsfw_gif(const std::string& file_content, const std::vector<std::size_t>& frames)
{
	dpp::json answer;
	bool first = true;

	decode_gif_frames(
		reinterpret_cast<const unsigned char*>(file_content.data()),
		file_content.size(),
		frames,
		[&answer, &first](std::size_t, const unsigned char* pixels, int width, int height) {
			if (static_cast<uint64_t>(width) * static_cast<uint64_t>(height) > max_pixels) {
				throw std::runtime_error("image_size");
			}

			Pix* image = rgba_to_pix(pixels, width, height);

			if (!image) {
				throw std::runtime_error("pix_create_failed");
			}

			std::string png;

			try {
				png = pix_to_png(image);
			} catch (...) {
				pixDestroy(&image);
				throw;
			}

			pixDestroy(&image);

			const dpp::json frame_answer = run_basic_nsfw(png);

			if (first) {
				answer = frame_answer;
				first = false;
				return;
			}

			for (const std::string& key : {"sexy", "porn", "drawing", "hentai"}) {
				if (frame_answer.at(key).get<double>() > answer.at(key).get<double>()) {
					answer[key] = frame_answer.at(key);
				}
			}
		}
	);

	if (first) {
		throw std::runtime_error("no_gif_frames");
	}

	return answer;
}

scan_result scan_basic_nsfw(const dpp::json& command, const std::string& file_content, const std::vector<std::size_t>& frames)
{
	scan_result result;
	result.scanner = "basic_nsfw";
	result.scanner_name = "NSFW Rules";

	const dpp::json settings = command.contains("basic_nsfw") && command.at("basic_nsfw").is_object() ? command.at("basic_nsfw") : dpp::json::object();

	const double suggestive_threshold = settings.contains("suggestive") ? settings.at("suggestive").get<double>() : 0.9;
	const double porn_threshold = settings.contains("porn") ? settings.at("porn").get<double>() : 0.9;
	const double drawing_threshold = settings.contains("drawing") ? settings.at("drawing").get<double>() : 0.0;
	const double hentai_threshold = settings.contains("hentai") ? settings.at("hentai").get<double>() : 0.9;

	if (suggestive_threshold == 0.0 &&
	    porn_threshold == 0.0 &&
	    drawing_threshold == 0.0 &&
	    hentai_threshold == 0.0) {
		result.text = "No match or not enabled";
		return result;
	}

	result.enabled = true;

	dpp::json answer;

	if (command.contains("cache") && command.at("cache").contains("basic") && command.at("cache").at("basic").is_object()) {
		answer = command.at("cache").at("basic");
	} else {
		try {
			answer = frames.empty() ? run_basic_nsfw(file_content) : run_basic_nsfw_gif(file_content, frames);
		} catch (const std::exception& e) {
			result.text = e.what();
			return result;
		}

		result.cache = answer;
	}

	result.raw = answer;

	auto check = [&result, &answer](const std::string& key, const std::string& label, double threshold) -> bool {
		if (threshold == 0.0) {
			return false;
		}

		const double score = answer.at(key).get<double>();

		if (score <= threshold) {
			return false;
		}

		result.blocked = true;
		result.text = fmt::format("NSFW: {} ({:.02f}{})", label, score * 100, '%');
		result.trigger = score;
		result.threshold = threshold;
		return true;
	};

	if (check("sexy", "Suggestive", suggestive_threshold)) {
		return result;
	}

	if (check("porn", "Pornography", porn_threshold)) {
		return result;
	}

	if (check("drawing", "Drawing", drawing_threshold)) {
		return result;
	}

	if (check("hentai", "Hentai", hentai_threshold)) {
		return result;
	}

	result.text = "No match";
	return result;
}

dpp::json object_merge(const dpp::json& lhs, const dpp::json& rhs)
{
	dpp::json merged = lhs;

	for (auto it = rhs.cbegin(); it != rhs.cend(); ++it) {
		const auto& key = it.key();

		if (it->is_object()) {
			if (lhs.contains(key)) {
				merged[key] = object_merge(lhs[key], rhs[key]);
			} else {
				merged[key] = rhs[key];
			}
		} else {
			merged[key] = it.value();
		}
	}

	return merged;
}


dpp::json result_to_json(const scan_result& result)
{
	dpp::json out = {
		{"scanner", result.scanner},
		{"scanner_name", result.scanner_name},
		{"enabled", result.enabled},
		{"blocked", result.blocked},
		{"text", result.text},
		{"trigger", result.trigger},
		{"threshold", result.threshold},
		{"raw", result.raw}
	};

	if (!result.cache.is_null()) {
		out["cache"] = result.cache;
	}

	return out;
}

dpp::json scan_all(const dpp::json& command, const std::string& hash, const std::string& file_content, const std::string& filename)
{
	const bool premium = json_bool(command, "premium", false);
	const bool scan_animation = premium && is_animated_gif(file_content);

	std::vector<std::size_t> frames;

	if (scan_animation) {
		frames = gif_frames_to_scan(
			reinterpret_cast<const unsigned char*>(file_content.data()),
			file_content.size()
		);
	}

	const std::string prepared_content = scan_animation ? file_content : flatten_gif(filename, file_content);

	std::vector<scan_result> results;

	try {
		results.emplace_back(scan_ocr(command, prepared_content, frames));
	} catch (const std::exception& e) {
		scan_result result;
		result.scanner = "ocr";
		result.scanner_name = "Text Recognition Rules";
		result.enabled = true;
		result.text = std::string("OCR Error: ") + e.what();
		results.emplace_back(result);
	}

	if (!results.back().blocked) {
		try {
			results.emplace_back(scan_basic_nsfw(command, prepared_content, frames));
		} catch (const std::exception& e) {
			scan_result result;
			result.scanner = "basic_nsfw";
			result.scanner_name = "NSFW Rules";
			result.enabled = true;
			result.text = std::string("NSFW Error: ") + e.what();
			results.emplace_back(result);
		}
	}

	dpp::json response = {
		{"stage", "scan"},
		{"status", "clean"},
		{"hash", hash},
		{"results", dpp::json::array()},
		{"cache", dpp::json::object()}
	};

	for (const scan_result& result : results) {
		response["results"].push_back(result_to_json(result));

		if (!result.cache.is_null()) {
			response["cache"][result.scanner] = result.cache;
		}

		if (result.blocked) {
			response["status"] = "blocked";
			response["scanner"] = result.scanner;
			response["scanner_name"] = result.scanner_name;
			response["text"] = result.text;
			response["trigger"] = result.trigger;
			response["threshold"] = result.threshold;
			break;
		}
	}

	return response;
}

int main()
{
	std::signal(SIGALRM, tessd_timeout);
	alarm(60);

	set_limit(RLIMIT_DATA, one_gigabyte);
	set_limit(RLIMIT_RSS, one_gigabyte);

	dpp::json request;

	if (!proc::read_frame(std::cin, request)) {
		write_error("fetch", "invalid_request");
		return static_cast<int>(tessd::exit_code::read);
	}

	if (!request.contains("action") || request.at("action") != "fetch") {
		write_error("fetch", "invalid_action");
		return static_cast<int>(tessd::exit_code::read);
	}

	if (!request.contains("url") || !request.at("url").is_string()) {
		write_error("fetch", "missing_url");
		return static_cast<int>(tessd::exit_code::read);
	}

	std::string file_content;
	std::string filename;

	if (request.contains("filename") && request.at("filename").is_string()) {
		filename = request.at("filename").get<std::string>();
	}

	try {
		if (!fetch_image(request.at("url").get<std::string>(), file_content)) {
			return static_cast<int>(tessd::exit_code::read);
		}
	} catch (const std::exception& e) {
		write_error("fetch", "exception", e.what());
		return static_cast<int>(tessd::exit_code::read);
	}

	const std::string hash = sha256(file_content);

	proc::write_frame({
		{"stage", "hash"},
		{"status", "ok"},
		{"hash", hash},
		{"size", file_content.size()}
	});

	dpp::json command;

	if (!proc::read_frame(std::cin, command)) {
		return static_cast<int>(tessd::exit_code::no_error);
	}

	if (!command.contains("action") || !command.at("action").is_string()) {
		write_error("command", "invalid_action");
		return static_cast<int>(tessd::exit_code::read);
	}

	const std::string action = command.at("action").get<std::string>();

	if (action == "stop") {
		return static_cast<int>(tessd::exit_code::no_error);
	}

	if (action != "continue") {
		write_error("command", "invalid_action");
		return static_cast<int>(tessd::exit_code::read);
	}

	try {
		proc::write_frame(scan_all(command, hash, file_content, filename));
	} catch (const std::exception& e) {
		write_error("scan", "exception", e.what());
		return static_cast<int>(tessd::exit_code::read);
	}

	return static_cast<int>(tessd::exit_code::no_error);
}

/************************************************************************************
 * 
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0
 *
 ************************************************************************************/
#include <dpp/dpp.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <beholder/beholder.h>
#include <beholder/config.h>
#include <beholder/proc/json_frame.h>
#include <beholder/proc/spawn.h>
#include <beholder/tessd.h>
#include "../src/3rdparty/httplib.h"
#include <CxxUrl/url.hpp>
#include <cctype>
#include <csignal>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <fmt/format.h>
#include <vector>

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

bool is_scannable_path(const std::string& path)
{
	const std::string lower = dpp::lowercase(path);

	return lower.ends_with(".webp")
	       || lower.ends_with(".jpg")
	       || lower.ends_with(".jpeg")
	       || lower.ends_with(".png")
	       || lower.ends_with(".gif");
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

	if (!is_scannable_path(parsed.path())) {
		write_error("fetch", "unsupported_file_type");
		return false;
	}

	httplib::Client cli(host.c_str());
	cli.enable_server_certificate_verification(false);
	cli.set_interface(config::get("tunnel_interface").get<std::string>());

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

	const auto* filebits = reinterpret_cast<const uint8_t*>(file_content.data());

	if (filebits[0] != 'G' || filebits[1] != 'I' || filebits[2] != 'F' || filebits[3] != '8' || filebits[4] != '9' || filebits[5] != 'a') {
		return false;
	}

	for (size_t pos = 0; pos + 3 < file_content.length(); ++pos) {
		if (filebits[pos] == 0x21 && filebits[pos + 1] == 0xF9 && filebits[pos + 2] == 0x04) {
			return true;
		}
	}

	return false;
}

std::string flatten_gif(const std::string& filename, const std::string& file_content)
{
	if (!filename.ends_with(".gif") && !filename.ends_with(".GIF")) {
		return file_content;
	}

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

std::string run_tesseract_image(Pix* image)
{
	tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();

	if (api->Init(nullptr, "eng", tesseract::OcrEngineMode::OEM_DEFAULT)) {
		delete api;
		throw std::runtime_error("tesseract_init_failed");
	}

	api->SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_BLOCK);
	api->SetImage(image);

	const char* output = api->GetUTF8Text();

	api->Clear();
	delete api;

	if (!output) {
		throw std::runtime_error("no_ocr_output");
	}

	std::string text = output;
	delete[] output;

	return text;
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

std::string run_tesseract(const std::string& file_content)
{
	Pix* image = pixReadMem(reinterpret_cast<const l_uint8*>(file_content.data()), file_content.size());

	if (!image) {
		throw std::runtime_error("pix_read_mem_failed");
	}

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

scan_result scan_ocr(const dpp::json& command, const std::string& file_content)
{
	scan_result result;
	result.scanner = "ocr";
	result.scanner_name = "Text Recognition Rules";

	const std::vector<std::string> patterns = command.contains("ocr_patterns")
						  ? json_string_array(command.at("ocr_patterns"))
						  : std::vector<std::string>{};

	if (patterns.empty()) {
		result.text = "No match or not enabled";
		return result;
	}

	result.enabled = true;

	std::string ocr_text;

	if (command.contains("cache") && command.at("cache").contains("ocr") && command.at("cache").at("ocr").is_string()) {
		ocr_text = command.at("cache").at("ocr").get<std::string>();
	} else {
		ocr_text = run_tesseract(file_content);
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

scan_result scan_basic_nsfw(const dpp::json& command, const std::string& file_content)
{
	scan_result result;
	result.scanner = "basic_nsfw";
	result.scanner_name = "Basic NSFW Rules";

	const dpp::json settings = command.contains("basic_nsfw") && command.at("basic_nsfw").is_object() ? command.at("basic_nsfw") : dpp::json::object();

	const bool check_suggestive = json_bool(settings, "suggestive", true);
	const bool check_porn = json_bool(settings, "porn", true);
	const bool check_drawing = json_bool(settings, "drawing", false);
	const bool check_hentai = json_bool(settings, "hentai", true);

	if (!check_suggestive && !check_porn && !check_drawing && !check_hentai) {
		result.text = "No match or not enabled";
		return result;
	}

	result.enabled = true;

	dpp::json answer;

	if (command.contains("cache") && command.at("cache").contains("basic") && command.at("cache").at("basic").is_object()) {
		answer = command.at("cache").at("basic");
	} else {
		httplib::Client cli("http://localhost:6969");
		auto res = cli.Post("/", file_content, "application/octet-stream");

		if (!res) {
			result.text = "Basic NSFW API Error: " + httplib::to_string(res.error());
			return result;
		}

		if (res->status >= 400) {
			result.text = "Basic NSFW API HTTP status " + std::to_string(res->status);
			return result;
		}

		answer = dpp::json::parse(res->body);

		if (answer.contains("error")) {
			result.text = "Basic NSFW API Error: " + answer.at("error").get<std::string>();
			return result;
		}

		result.cache = answer;
	}

	result.raw = answer;

	if (check_suggestive) {
		const double score = answer.at("sexy").get<double>();

		if (score > 0.8) {
			result.blocked = true;
			result.text = fmt::format("Basic NSFW: Suggestive ({0:.02f}{1})", score * 100, '%');
			result.trigger = score;
			result.threshold = 0.8;
			return result;
		}
	}

	if (check_porn) {
		const double score = answer.at("porn").get<double>();

		if (score > 0.8) {
			result.blocked = true;
			result.text = fmt::format("Basic NSFW: Pornography ({0:.02f}{1})", score * 100, '%');
			result.trigger = score;
			result.threshold = 0.8;
			return result;
		}
	}

	if (check_drawing) {
		const double score = answer.at("drawing").get<double>();

		if (score > 0.95) {
			result.blocked = true;
			result.text = fmt::format("Basic NSFW: Drawing ({0:.02f}{1})", score * 100, '%');
			result.trigger = score;
			result.threshold = 0.95;
			return result;
		}
	}

	if (check_hentai) {
		const double score = answer.at("hentai").get<double>();

		if (score > 0.8) {
			result.blocked = true;
			result.text = fmt::format("Basic NSFW: Hentai ({0:.02f}{1})", score * 100, '%');
			result.trigger = score;
			result.threshold = 0.8;
			return result;
		}
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
	const std::string prepared_content = flatten_gif(filename, file_content);

	std::vector<scan_result> results;

	try {
		results.emplace_back(scan_ocr(command, prepared_content));
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
			results.emplace_back(scan_basic_nsfw(command, prepared_content));
		} catch (const std::exception& e) {
			scan_result result;
			result.scanner = "basic_nsfw";
			result.scanner_name = "Basic NSFW Rules";
			result.enabled = true;
			result.text = std::string("Basic NSFW Error: ") + e.what();
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
	config::init("../config.json");

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
/************************************************************************************
 * 
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023,2026 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#include <dpp/dpp.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <beholder/beholder.h>
#include <beholder/proc/json_frame.h>
#include <beholder/tessd.h>
#include "3rdparty/httplib.h"
#include <beholder/trusted_hosts.h>
#include <CxxUrl/url.hpp>
#include <cctype>
#include <csignal>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <fmt/format.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>

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
 *
 * Whilst OCR was the original reason for this process existing, it now also handles other
 * image analysis tasks. It downloads media from URLs, calculates SHA-256 hashes for cache
 * lookups, performs local NSFW classification, and scans animated GIFs and MP4 videos by
 * selecting only perceptually distinct frames rather than processing every single frame.
 * This keeps the expensive image processing code together in one place.
 *
 * Keeping all of this work in a separate process also improves robustness. The worker runs
 * under strict memory and execution time limits, so if a buggy image decoder, OCR engine or
 * malformed input causes it to fail, the main bot process remains unaffected and can simply
 * launch another worker.
 *
 * The worker communicates with the parent process using a simple length-prefixed JSON protocol
 * over stdin/stdout. This also makes it easy to test standalone from the command line without
 * having to run the rest of Beholder.
 */

constexpr uint64_t one_gigabyte = 1073741824ULL;
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

bool json_bool(const dpp::json& value, const std::string& key, bool fallback);

std::vector<std::string> json_string_array(const dpp::json& value);

std::string tesseract_language_code(const std::string& language)
{
	static const std::unordered_map<std::string, std::string> languages = {
		{"af", "afr"},
		{"am", "amh"},
		{"ar", "ara"},
		{"az", "aze"},
		{"be", "bel"},
		{"bg", "bul"},
		{"ca", "cat"},
		{"cs", "ces"},
		{"cy", "cym"},
		{"da", "dan"},
		{"de", "deu"},
		{"el", "ell"},
		{"en", "eng"},
		{"eo", "epo"},
		{"es", "spa"},
		{"et", "est"},
		{"eu", "eus"},
		{"fa", "fas"},
		{"fi", "fin"},
		{"fr", "fra"},
		{"gd", "gla"},
		{"gl", "glg"},
		{"hi", "hin"},
		{"hr", "hrv"},
		{"hy", "hye"},
		{"id", "ind"},
		{"is", "isl"},
		{"it", "ita"},
		{"ja", "jpn"},
		{"kh", "khm"},
		{"ko", "kor"},
		{"la", "lat"},
		{"lt", "lit"},
		{"lv", "lav"},
		{"mi", "mri"},
		{"mk", "mkd"},
		{"ml", "mal"},
		{"mn", "mon"},
		{"mr", "mar"},
		{"ms", "msa"},
		{"mt", "mlt"},
		{"my", "mya"},
		{"nl", "nld"},
		{"no", "nor"},
		{"pl", "pol"},
		{"pt", "por"},
		{"ro", "ron"},
		{"ru", "rus"},
		{"sk", "slk"},
		{"sl", "slv"},
		{"sq", "sqi"},
		{"sr", "srp"},
		{"sv", "swe"},
		{"ta", "tam"},
		{"te", "tel"},
		{"th", "tha"},
		{"to", "ton"},
		{"tr", "tur"},
		{"uk", "ukr"},
		{"uz", "uzb"},
		{"vi", "vie"},
		{"yid", "yid"},
		{"zh", "chi_sim"},
	};
	const auto found = languages.find(language);
	return found == languages.end() ? "" : found->second;
}

std::string get_tesseract_languages(const dpp::json& command)
{
	std::vector<std::string> languages{"eng"};
	std::unordered_set<std::string> added{"eng"};

	if (!json_bool(command, "premium", false) || !command.contains("prem_languages")) {
		return "eng";
	}

	for (const std::string& language : json_string_array(command.at("prem_languages"))) {
		const std::string mapped = tesseract_language_code(language);

		if (!mapped.empty() && !added.contains(mapped)) {
			languages.push_back(mapped);
			added.emplace(mapped);
		}
	}

	std::string result;

	for (const std::string& language : languages) {
		if (!result.empty()) {
			result += "+";
		}

		result += language;
	}

	return result;
}

Pix* rgba_to_pix(const unsigned char* pixels, int width, int height, int stride = 0)
{
	if (!pixels || width <= 0 || height <= 0) {
		return nullptr;
	}

	const int actual_stride = (stride > 0) ? stride : (width * 4);
	Pix* image = pixCreate(width, height, 32);

	if (!image) {
		return nullptr;
	}

	l_uint32* destination = pixGetData(image);
	const int words_per_line = pixGetWpl(image);

	for (int y = 0; y < height; ++y) {
		l_uint32* destination_line = destination + (y * words_per_line);
		const unsigned char* source_line = pixels + (static_cast<std::size_t>(y) * static_cast<std::size_t>(actual_stride));

		for (int x = 0; x < width; ++x) {
			const unsigned char* source = source_line + (x * 4);
			composeRGBPixel(source[0], source[1], source[2], &destination_line[x]);
		}
	}

	return image;
}

void tessd_timeout(int) {
	static constexpr auto timeout_frame = proc::make_json_frame("{\"stage\":\"alarm\",\"status\":\"error\",\"error\":\"timeout\"}\n");
	::write(STDOUT_FILENO, timeout_frame.data(), timeout_frame.size() - 1);
	::_exit(static_cast<int>(tessd::exit_code::timeout));
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

bool trusted_media_host(const std::string& host)
{
	for (const char* trusted_host : trusted) {
		if (match(host.c_str(), trusted_host)) {
			return true;
		}
	}

	return false;
}

bool fetch_image(const std::string& url, std::string& file_content)
{
	Url parsed(url);
	const std::string host = parsed.scheme() + "://" + parsed.host();
	const std::string path = build_path_with_query(parsed);

	httplib::Client cli(host.c_str());

	cli.set_default_headers({
		{ "User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36" },
		{ "Accept", "image/*,video/*,*/*;q=0.8" },
		{ "Accept-Language", "en-GB,en;q=0.9" },
		{ "Cache-Control", "no-cache" },
		{ "Pragma", "no-cache" },
		{ "Referer", host + "/" },
	});
	cli.set_keep_alive(true);
	cli.set_follow_location(true);
	cli.set_read_timeout(5);

	if (!trusted_media_host(host)) {
		cli.enable_server_certificate_verification(false);
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

	if (!is_webm(res->body) && !is_mp4(res->body) && !is_webp(res->body) && !is_avif(res->body) && !validate_image_dimensions(res->body)) {
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

static std::string run_tesseract_image_with_psm(Pix* image, tesseract::PageSegMode psm, const std::string& languages)
{
	/**
	 * RAII isn't a thing in tesseract land. We can't just initialise the object
	 * by `new`, we have to separately call an Init method. One of many
	 * anti-patterns.
	 */
	tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();

	if (api->Init(nullptr, languages.c_str(), tesseract::OcrEngineMode::OEM_DEFAULT)) {
		if (languages != "eng" && !api->Init(nullptr, "eng", tesseract::OcrEngineMode::OEM_DEFAULT)) {
			// Fell back to English because one or more requested language models were unavailable.
		} else {
			delete api;
			throw std::runtime_error("tesseract_init_failed");
		}
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

std::string run_tesseract_image(Pix* image, const std::string& languages)
{
	std::string block_text = run_tesseract_image_with_psm(image, tesseract::PageSegMode::PSM_SINGLE_BLOCK, languages);
	std::string sparse_text = run_tesseract_image_with_psm(image, tesseract::PageSegMode::PSM_SPARSE_TEXT, languages);

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


std::string run_tesseract_gif(const std::string& file_content, const std::vector<std::size_t>& frames, const std::string& languages)
{
	std::string text;

	decode_gif_frames(
		reinterpret_cast<const unsigned char*>(file_content.data()),
		file_content.size(),
		frames,
		[&text,&languages](std::size_t, const unsigned char* pixels, int width, int height) {
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
				frame_text = run_tesseract_image(image, languages);
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

std::string run_tesseract_mp4(const std::string& file_content, const std::vector<std::size_t>& frames, const std::string& languages)
{
	std::string text;

	decode_mp4_frames(
		reinterpret_cast<const unsigned char*>(file_content.data()),
		file_content.size(),
		frames,
		[&text,&languages](std::size_t, const unsigned char* pixels, int width, int height) {
			if (static_cast<uint64_t>(width) * static_cast<uint64_t>(height) > max_pixels) {
				throw std::runtime_error("image_size");
			}

			Pix* image = rgba_to_pix(pixels, width, height);

			if (!image) {
				throw std::runtime_error("pix_create_failed");
			}

			std::string frame_text;

			try {
				frame_text = run_tesseract_image(image, languages);
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

std::string run_tesseract(const std::string& file_content, const std::string& languages)
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

	std::string text = run_tesseract_image(image, languages);

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

	text = run_tesseract_image(shaped, languages);
	pixDestroy(&shaped);

	return text;
}

std::string run_profanity_filter(const std::string& text, const std::vector<std::string>& languages)
{
	httplib::Client cli("http://localhost:6970");

	const dpp::json payload = {
		{"content", text},
		{"censor-character", "*"},
		{"languages", languages}
	};

	auto res = cli.Post("/bad-word-filter", payload.dump(), "application/json");

	if (!res) {
		throw std::runtime_error("Profanity API Error: " + httplib::to_string(res.error()));
	}

	if (res->status >= 400) {
		throw std::runtime_error("Profanity API HTTP status " + std::to_string(res->status));
	}

	dpp::json answer = dpp::json::parse(res->body);

	if (answer.contains("error")) {
		throw std::runtime_error("Profanity API Error: " + answer.at("error").get<std::string>());
	}

	if (!answer.contains("censored-content") || !answer.at("censored-content").is_string()) {
		throw std::runtime_error("Profanity API returned no censored content");
	}

	return answer.at("censored-content").get<std::string>();
}

scan_result scan_ocr(const dpp::json& command, const std::string& file_content, const std::vector<std::size_t>& frames, bool mp4, bool webp, bool avif)
{
	scan_result result;
	result.scanner = "ocr";
	result.scanner_name = "Text Recognition Rules";

	const std::vector<std::string> patterns = command.contains("ocr_patterns") ? json_string_array(command.at("ocr_patterns")) : std::vector<std::string>{};
	const bool profanity_enabled = json_bool(command, "prem_profanity_filter_enable", false);
	const std::vector<std::string> languages = command.contains("prem_languages") ? json_string_array(command.at("prem_languages")) : std::vector<std::string>{};
	std::string languages_str = get_tesseract_languages(command);

	if (patterns.empty() && (!profanity_enabled || languages.empty())) {
		result.text = "No match or not enabled";
		return result;
	}

	result.enabled = true;

	std::string ocr_text;

	if (command.contains("cache") && command.at("cache").contains("ocr") && command.at("cache").at("ocr").is_string()) {
		ocr_text = command.at("cache").at("ocr").get<std::string>();
	} else if (frames.empty()) {
		ocr_text = run_tesseract(file_content, languages_str);
		result.cache = ocr_text;
	} else if (mp4) {
		ocr_text = run_tesseract_mp4(file_content, frames, languages_str);
		result.cache = ocr_text;
	} else if (webp) {
		ocr_text = run_tesseract_webp(file_content, frames, languages_str);
		result.cache = ocr_text;
	} else if (avif) {
		ocr_text = run_tesseract_avif(file_content, frames, languages_str);
		result.cache = ocr_text;
	} else {
		ocr_text = run_tesseract_gif(file_content, frames, languages_str);
		result.cache = ocr_text;
	}

	result.raw = {
		{"text", ocr_text}
	};

	if (profanity_enabled && !languages.empty() && has_text(ocr_text)) {
		try {
			result.raw["censored_text"] = run_profanity_filter(ocr_text, languages);
		} catch (const std::exception& e) {
			result.raw["profanity_error"] = e.what();
		}
	}

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

dpp::json run_basic_nsfw_mp4(const std::string& file_content, const std::vector<std::size_t>& frames)
{
	dpp::json answer;
	bool first = true;

	decode_mp4_frames(
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

			for (const std::string key : {"sexy", "porn", "drawing", "hentai"}) {
				if (frame_answer.at(key).get<double>() > answer.at(key).get<double>()) {
					answer[key] = frame_answer.at(key);
				}
			}
		}
	);

	if (first) {
		throw std::runtime_error("no_mp4_frames");
	}

	return answer;
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

			const std::string png = rgba_to_png(pixels, width, height);

			const dpp::json frame_answer = run_basic_nsfw(png);

			if (first) {
				answer = frame_answer;
				first = false;
				return;
			}

			for (const std::string key : {"sexy", "porn", "drawing", "hentai"}) {
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

scan_result scan_basic_nsfw(const dpp::json& command, const std::string& file_content, const std::vector<std::size_t>& frames, bool mp4, bool webp, bool avif)
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
			if (frames.empty()) {
				answer = run_basic_nsfw(file_content);
			} else if (mp4) {
				answer = run_basic_nsfw_mp4(file_content, frames);
			} else if (webp) {
				answer = run_basic_nsfw_webp(file_content, frames);
			} else if (avif) {
				answer = run_basic_nsfw_avif(file_content, frames);
			} else {
				answer = run_basic_nsfw_gif(file_content, frames);
			}
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
		result.text = fmt::format(fmt::runtime("NSFW: {} ({:.02f}{})"), label, score * 100, '%');
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

std::string run_tesseract_webp(const std::string& file_content, const std::vector<std::size_t>& frames, const std::string& languages)
{
	std::string text;

	decode_webp_frames(
		reinterpret_cast<const unsigned char*>(file_content.data()),
		file_content.size(),
		frames,
		[&text,&languages](std::size_t, const unsigned char* pixels, int width, int height) {
			if (static_cast<uint64_t>(width) * static_cast<uint64_t>(height) > max_pixels) {
				throw std::runtime_error("image_size");
			}

			Pix* image = rgba_to_pix(pixels, width, height);

			if (!image) {
				throw std::runtime_error("pix_create_failed");
			}

			std::string frame_text;

			try {
				frame_text = run_tesseract_image(image, languages);
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

dpp::json run_basic_nsfw_webp(const std::string& file_content, const std::vector<std::size_t>& frames)
{
	dpp::json answer;
	bool first = true;

	decode_webp_frames(
		reinterpret_cast<const unsigned char*>(file_content.data()),
		file_content.size(),
		frames,
		[&answer, &first](std::size_t, const unsigned char* pixels, int width, int height) {
			if (static_cast<uint64_t>(width) * static_cast<uint64_t>(height) > max_pixels) {
				throw std::runtime_error("image_size");
			}

			const dpp::json frame_answer = run_basic_nsfw(rgba_to_png(pixels, width, height));

			if (first) {
				answer = frame_answer;
				first = false;
				return;
			}

			for (const std::string key : {"sexy", "porn", "drawing", "hentai"}) {
				if (frame_answer.at(key).get<double>() > answer.at(key).get<double>()) {
					answer[key] = frame_answer.at(key);
				}
			}
		}
	);

	if (first) {
		throw std::runtime_error("no_webp_frames");
	}

	return answer;
}

std::string run_tesseract_avif(const std::string& file_content, const std::vector<std::size_t>& frames, const std::string& languages)
{
	std::string text;

	decode_avif_frames(
		reinterpret_cast<const unsigned char*>(file_content.data()),
		file_content.size(),
		frames,
		[&text, &languages](std::size_t, const unsigned char* pixels, int width, int height) {
			if (static_cast<uint64_t>(width) * static_cast<uint64_t>(height) > max_pixels) {
				throw std::runtime_error("image_size");
			}

			Pix* image = rgba_to_pix(pixels, width, height);

			if (!image) {
				throw std::runtime_error("pix_create_failed");
			}

			std::string frame_text;

			try {
				frame_text = run_tesseract_image(image, languages);
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

dpp::json run_basic_nsfw_avif(const std::string& file_content, const std::vector<std::size_t>& frames)
{
	dpp::json answer;
	bool first = true;

	decode_avif_frames(
		reinterpret_cast<const unsigned char*>(file_content.data()),
		file_content.size(),
		frames,
		[&answer, &first](std::size_t, const unsigned char* pixels, int width, int height) {
			if (static_cast<uint64_t>(width) * static_cast<uint64_t>(height) > max_pixels) {
				throw std::runtime_error("image_size");
			}

			const dpp::json frame_answer = run_basic_nsfw(rgba_to_png(pixels, width, height));

			if (first) {
				answer = frame_answer;
				first = false;
				return;
			}

			for (const std::string key : {"sexy", "porn", "drawing", "hentai"}) {
				if (frame_answer.at(key).get<double>() > answer.at(key).get<double>()) {
					answer[key] = frame_answer.at(key);
				}
			}
		}
	);

	if (first) {
		throw std::runtime_error("no_avif_frames");
	}

	return answer;
}

dpp::json scan_all(const dpp::json& command, const std::string& hash, const std::string& file_content, const std::string& filename)
{
	const bool premium = json_bool(command, "premium", false);
	const bool animated_scan_enabled = premium && json_bool(command, "prem_anim_scan_enable", true);
	const bool video_scan_enabled = premium && json_bool(command, "prem_video_scan_enable", true);
	const bool webp = is_webp(file_content);
	const bool avif = is_avif(file_content);
	const bool scan_gif = animated_scan_enabled && is_animated_gif(file_content);
	const bool scan_webp = animated_scan_enabled && webp && is_animated_webp(file_content);
	const bool scan_avif = animated_scan_enabled && avif && is_animated_avif(file_content);
	const bool scan_mp4 = video_scan_enabled && !avif && (is_mp4(file_content) || is_webm(file_content));

	std::vector<std::size_t> frames;

	if (scan_gif) {
		frames = gif_frames_to_scan(reinterpret_cast<const unsigned char*>(file_content.data()), file_content.size());
	} else if (scan_webp) {
		frames = webp_frames_to_scan(reinterpret_cast<const unsigned char*>(file_content.data()), file_content.size());
	} else if (scan_avif) {
		frames = avif_frames_to_scan(reinterpret_cast<const unsigned char*>(file_content.data()), file_content.size());
	} else if (scan_mp4) {
		frames = mp4_frames_to_scan(reinterpret_cast<const unsigned char*>(file_content.data()), file_content.size());
	}

	std::string prepared_content;

	if (scan_gif || scan_webp || scan_avif || scan_mp4) {
		prepared_content = file_content;
	} else if (webp) {
		prepared_content = flatten_webp(file_content);
	} else if (avif) {
		prepared_content = flatten_avif(file_content);
	} else {
		prepared_content = flatten_gif(filename, file_content);
	}

	std::vector<scan_result> results;

	try {
		results.emplace_back(scan_ocr(command, prepared_content, frames, scan_mp4, scan_webp, scan_avif));
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
			results.emplace_back(scan_basic_nsfw(command, prepared_content, frames, scan_mp4, scan_webp, scan_avif));
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

int main(int argc, char** argv)
{
	std::signal(SIGALRM, tessd_timeout);
	alarm(60);

	// Shut up, leptonica!
	close(STDERR_FILENO);

	set_limit(RLIMIT_DATA, one_gigabyte);
	set_limit(RLIMIT_RSS, one_gigabyte);

	if (argc > 1) {
		return tessd_cli(argc, argv);
	}

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

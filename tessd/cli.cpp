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
#include <beholder/tessd.h>
#include "3rdparty/httplib.h"
#include <CxxUrl/url.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

std::string cli_build_path_with_query(const Url& url)
{
	std::string path = url.path();

	if (!url.query().empty()) {
		std::stringstream query;
		bool first = true;

		for (const auto& parameter : url.query()) {
			if (!first) {
				query << "&";
			}

			query << parameter.key() << "=" << parameter.val();
			first = false;
		}

		path += "?" + query.str();
	}

	return path;
}

bool cli_fetch_image(const std::string& url, std::string& file_content)
{
	Url parsed(url);
	const std::string host = parsed.scheme() + "://" + parsed.host();
	const std::string path = cli_build_path_with_query(parsed);

	httplib::Client client(host.c_str());
	client.enable_server_certificate_verification(false);
	auto response = client.Get(path);

	if (!response) {
		std::cerr << "Download failed: " << httplib::to_string(response.error()) << '\n';
		return false;
	}

	if (response->status >= 400) {
		std::cerr << "HTTP status: " << response->status << '\n';
		return false;
	}

	file_content = std::move(response->body);
	return true;
}

int tessd_cli(int argc, char** argv)
{
	if (argc < 3) {
		std::cerr << "Usage:\n";
		std::cerr << "  tessd gif-frames <url> [threshold] [--frames-only]\n";
		std::cerr << "  tessd mp4-frames <url> [threshold] [--frames-only]\n";
		return 1;
	}

	const std::string command = argv[1];

	if (command != "gif-frames" && command != "mp4-frames") {
		std::cerr << "Unknown command: " << command << '\n';
		return 1;
	}

	double threshold = command == "gif-frames" ? 6.0  : 12.0;
	bool frames_only = false;

	for (int arg = 3; arg < argc; ++arg) {
		const std::string value = argv[arg];

		if (value == "--frames-only") {
			frames_only = true;
			continue;
		}

		char* end = nullptr;
		const double parsed = std::strtod(value.c_str(), &end);

		if (!end || *end != '\0') {
			std::cerr << "Invalid argument: " << value << '\n';
			return 1;
		}

		threshold = parsed;
	}

	std::string file_content;

	if (!cli_fetch_image(argv[2], file_content)) {
		return 1;
	}

	try {
		std::size_t total_frames = 0;
		std::vector<std::size_t> frames;

		if (command == "gif-frames") {
			frames = gif_frames_to_scan(reinterpret_cast<const unsigned char*>(file_content.data()), file_content.size(), threshold, &total_frames);
		} else {
			frames = mp4_frames_to_scan( reinterpret_cast<const unsigned char*>(file_content.data()), file_content.size(), threshold, &total_frames);
		}

		std::cout << "Total frames: " << total_frames << '\n';
		std::cout << "Selected frames: " << frames.size() << '\n';
		std::cout << "Threshold: " << threshold << '\n';
		std::cout << "Frame indices:";

		for (std::size_t frame : frames) {
			std::cout << ' ' << frame;
		}

		std::cout << '\n';

		if (frames.empty()) {
			std::cerr << "No video frames decoded\n";
			return 1;
		}

		if (frames_only) {
			return 0;
		}

		std::cout << "\nOCR:\n";

		const std::string ocr_text = command == "gif-frames" ? run_tesseract_gif(file_content, frames, {"en"}) : run_tesseract_mp4(file_content, frames, {"en"});

		if (ocr_text.empty()) {
			std::cout << "(no text found)\n";
		} else {
			std::cout << ocr_text << '\n';
		}

		std::cout << "\nNSFW:\n";

		const dpp::json nsfw = command == "gif-frames" ? run_basic_nsfw_gif(file_content, frames) : run_basic_nsfw_mp4(file_content, frames);

		std::cout << nsfw.dump(2) << '\n';
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << '\n';
		return 1;
	}

	return 0;
}

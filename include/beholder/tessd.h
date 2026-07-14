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
#pragma once
#include <vector>
#include <cstddef>
#include <functional>
#include <stdlib.h>
#include <dpp/json.h>
#include <string>

namespace tessd {

	enum class exit_code : int {
		no_error =	0,
		read =		1,
		tess_init =	2,
		pix_read_mem =	3,
		image_size =	4,
		no_output =	5,
		timeout =	6,
		waitpid = 	7,
		exec_fail =	8,
		max = 		9,
	};

	inline const char* tessd_error[static_cast<int>(exit_code::max) + 1] = {
		"No error",
		"Error reading from STDIN",
		"Error initialising Tesseract",
		"Error reading image",
		"Image dimensions too large",
		"No OCR output",
		"Program timeout",
		"waitpid() on tessd failed",
		"exec() failed to launch tessd",
		"", // Marks the end of the array!
	};

	inline void status(exit_code e) {
		exit(static_cast<int>(e));
	}

};

using gif_frame_callback = std::function<void(std::size_t, const unsigned char*, int, int)>;

std::vector<std::size_t> gif_frames_to_scan(const unsigned char* gif_data, std::size_t gif_size, double threshold = 6.0, std::size_t* total_frames = nullptr);
void decode_gif_frames(const unsigned char* gif_data, std::size_t gif_size, const std::vector<std::size_t>& frames, const gif_frame_callback& callback);

int tessd_cli(int argc, char** argv);

std::string run_tesseract_gif(const std::string& file_content, const std::vector<std::size_t>& frames);
dpp::json run_basic_nsfw_gif(const std::string& file_content, const std::vector<std::size_t>& frames);
std::string run_tesseract_mp4(const std::string& file_content, const std::vector<std::size_t>& frames);
dpp::json run_basic_nsfw_mp4(const std::string& file_content, const std::vector<std::size_t>& frames);

std::vector<std::size_t> mp4_frames_to_scan(const unsigned char* mp4_data, std::size_t mp4_size, double threshold = 12.0, std::size_t* total_frames = nullptr);
void decode_mp4_frames(const unsigned char* mp4_data, std::size_t mp4_size, const std::vector<std::size_t>& frames, const gif_frame_callback& callback);


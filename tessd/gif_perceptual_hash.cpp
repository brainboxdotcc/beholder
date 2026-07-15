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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <nsfwd/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "3rdparty/stb_image_write.h"
#include <opencv2/core.hpp>
#include <opencv2/img_hash.hpp>
#include <opencv2/imgproc.hpp>
#include "beholder/tessd.h"

constexpr std::size_t max_gif_scan_frames = 250;


void png_write_callback(void* context, void* data, int size)
{
	auto* output = static_cast<std::string*>(context);
	output->append(static_cast<const char*>(data), static_cast<std::size_t>(size));
}

std::string rgba_to_png(const unsigned char* pixels, int width, int height)
{
	if (!pixels || width <= 0 || height <= 0) {
		throw std::runtime_error("invalid_frame");
	}

	std::string output;

	if (!stbi_write_png_to_func(png_write_callback, &output, width, height, 4, pixels, width * 4)) {
		throw std::runtime_error("png_encode_failed");
	}

	return output;
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

	stbi__context gif_context{};
	stbi__gif gif_state{};
	int components = 0;

	stbi__start_mem(&gif_context, reinterpret_cast<const unsigned char*>(file_content.data()), static_cast<int>(file_content.size()));

	unsigned char* pixels = stbi__gif_load_next(&gif_context, &gif_state, &components, STBI_rgb_alpha, nullptr);

	if (!pixels || pixels == reinterpret_cast<unsigned char*>(&gif_context) || gif_state.w <= 0 || gif_state.h <= 0) {
		STBI_FREE(gif_state.out);
		STBI_FREE(gif_state.history);
		STBI_FREE(gif_state.background);
		return file_content;
	}

	std::string flattened;

	try {
		flattened = rgba_to_png(pixels, gif_state.w, gif_state.h);
	} catch (...) {
		STBI_FREE(gif_state.out);
		STBI_FREE(gif_state.history);
		STBI_FREE(gif_state.background);
		return file_content;
	}

	STBI_FREE(gif_state.out);
	STBI_FREE(gif_state.history);
	STBI_FREE(gif_state.background);

	if (flattened.empty()) {
		return file_content;
	}

	return flattened;
}


/*
 * This must be compiled in the same translation unit as STB_IMAGE_IMPLEMENTATION
 * because it deliberately uses stb_image's internal GIF decoder API.
 */
std::vector<std::size_t> gif_frames_to_scan(const unsigned char* gif_data, std::size_t gif_size, double threshold, std::size_t* total_frames)
{
	if (!gif_data || gif_size == 0 || gif_size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
		throw std::invalid_argument("Invalid GIF data");
	}

	stbi__context gif_context {};
	stbi__gif gif_state {};

	stbi__start_mem(&gif_context, gif_data, static_cast<int>(gif_size));

	cv::Ptr<cv::img_hash::PHash> hasher = cv::img_hash::PHash::create();
	cv::Mat previous_hash;
	std::vector<std::size_t> frames;

	std::size_t frame_index = 0;
	int components = 0;

	while (true) {
		unsigned char* pixels = stbi__gif_load_next(&gif_context, &gif_state, &components, STBI_rgb_alpha, nullptr);
		if (pixels == reinterpret_cast<unsigned char*>(&gif_context)) {
			break;
		}

		if (!pixels) {
			break;
		}

		if (gif_state.w <= 0 || gif_state.h <= 0) {
			break;
		}

		/*
		 * stbi__gif_load_next() returns gif_state.out, which is the full
		 * composited GIF canvas after applying this frame.
		 *
		 * OpenCV does not own this memory. The Mat is only a temporary view.
		 */
		cv::Mat rgba(gif_state.h, gif_state.w, CV_8UC4, pixels);
		cv::Mat greyscale;
		cv::Mat current_hash;

		cv::cvtColor(rgba, greyscale, cv::COLOR_RGBA2GRAY);
		hasher->compute(greyscale, current_hash);

		bool scan = previous_hash.empty();

		if (!scan) {
			double distance = hasher->compare(previous_hash, current_hash);
			scan = distance >= threshold;
		}

		if (scan) {
			frames.push_back(frame_index);
			current_hash.copyTo(previous_hash);

			if (frames.size() >= max_gif_scan_frames) {
				break;
			}
		}

		++frame_index;
	}

	STBI_FREE(gif_state.out);
	STBI_FREE(gif_state.history);
	STBI_FREE(gif_state.background);

	if (total_frames) {
		*total_frames = frame_index;
	}

	return frames;
}

void decode_gif_frames(const unsigned char* gif_data, std::size_t gif_size, const std::vector<std::size_t>& frames, const animation_frame_callback& callback)
{
	if (!gif_data || gif_size == 0 || gif_size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
		throw std::invalid_argument("Invalid GIF data");
	}

	if (frames.empty()) {
		return;
	}

	stbi__context gif_context{};
	stbi__gif gif_state{};

	stbi__start_mem(&gif_context, gif_data, static_cast<int>(gif_size));

	std::size_t frame_index = 0;
	std::size_t selected_index = 0;
	int components = 0;

	try {
		while (selected_index < frames.size()) {
			unsigned char* pixels = stbi__gif_load_next(&gif_context, &gif_state, &components, STBI_rgb_alpha, nullptr);
			if (!pixels) {
				throw std::runtime_error("GIF ended before all selected frames were decoded");
			}

			if (pixels == reinterpret_cast<unsigned char*>(&gif_context)) {
				throw std::runtime_error("GIF ended before all selected frames were decoded");
			}

			if (gif_state.w <= 0 || gif_state.h <= 0) {
				throw std::runtime_error("Invalid GIF frame dimensions");
			}

			if (frame_index == frames[selected_index]) {
				callback(frame_index, pixels, gif_state.w, gif_state.h);
				++selected_index;
			}

			++frame_index;
		}
	} catch (...) {
		STBI_FREE(gif_state.out);
		STBI_FREE(gif_state.history);
		STBI_FREE(gif_state.background);
		throw;
	}

	STBI_FREE(gif_state.out);
	STBI_FREE(gif_state.history);
	STBI_FREE(gif_state.background);
}
/************************************************************************************
 *
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023,2026 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0
 *
 ************************************************************************************/
#include <beholder/tessd.h>
#include <avif/avif.h>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/img_hash.hpp>
#include <opencv2/imgproc.hpp>

constexpr std::size_t max_avif_scan_frames = 100;

std::string avif_error(avifResult result)
{
	return avifResultToString(result);
}

avifDecoder* open_avif_decoder(const unsigned char* avif_data, std::size_t avif_size)
{
	if (!avif_data || avif_size == 0) {
		throw std::invalid_argument("Invalid AVIF data");
	}

	avifDecoder* decoder = avifDecoderCreate();

	if (!decoder) {
		throw std::runtime_error("avif_decoder_create_failed");
	}

	const avifResult io_result = avifDecoderSetIOMemory(decoder, avif_data, avif_size);

	if (io_result != AVIF_RESULT_OK) {
		const std::string error = avif_error(io_result);
		avifDecoderDestroy(decoder);
		throw std::runtime_error("avif_decoder_io_failed: " + error);
	}

	const avifResult parse_result = avifDecoderParse(decoder);

	if (parse_result != AVIF_RESULT_OK) {
		const std::string error = avif_error(parse_result);
		avifDecoderDestroy(decoder);
		throw std::runtime_error("avif_decoder_parse_failed: " + error);
	}

	return decoder;
}

bool is_avif(const std::string& file_content)
{
	if (file_content.size() < 12) {
		return false;
	}

	avifROData data{};
	data.data = reinterpret_cast<const uint8_t*>(file_content.data());
	data.size = file_content.size();

	return avifPeekCompatibleFileType(&data) == AVIF_TRUE;
}

bool is_animated_avif(const std::string& file_content)
{
	if (!is_avif(file_content)) {
		return false;
	}

	avifDecoder* decoder = nullptr;

	try {
		decoder = open_avif_decoder(reinterpret_cast<const unsigned char*>(file_content.data()), file_content.size());
		const bool animated = decoder->imageCount > 1;
		avifDecoderDestroy(decoder);
		return animated;
	} catch (...) {
		if (decoder) {
			avifDecoderDestroy(decoder);
		}

		return false;
	}
}

void avif_frame_to_rgba(avifDecoder* decoder, std::vector<unsigned char>& rgba, int& width, int& height)
{
	if (!decoder || !decoder->image || decoder->image->width == 0 || decoder->image->height == 0) {
		throw std::runtime_error("invalid_avif_dimensions");
	}

	if (decoder->image->width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
	    decoder->image->height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
		throw std::runtime_error("invalid_avif_dimensions");
	}

	width = static_cast<int>(decoder->image->width);
	height = static_cast<int>(decoder->image->height);

	avifRGBImage rgb{};
	avifRGBImageSetDefaults(&rgb, decoder->image);
	rgb.format = AVIF_RGB_FORMAT_RGBA;
	rgb.depth = 8;

	const std::size_t row_size = static_cast<std::size_t>(width) * 4;
	const std::size_t buffer_size = row_size * static_cast<std::size_t>(height);

	if (row_size / 4 != static_cast<std::size_t>(width) ||
	    buffer_size / static_cast<std::size_t>(height) != row_size) {
		throw std::runtime_error("avif_frame_size_overflow");
	}

	rgba.resize(buffer_size);
	rgb.pixels = rgba.data();
	rgb.rowBytes = static_cast<uint32_t>(row_size);

	const avifResult convert_result = avifImageYUVToRGB(decoder->image, &rgb);

	if (convert_result != AVIF_RESULT_OK) {
		throw std::runtime_error("avif_rgb_conversion_failed: " + avif_error(convert_result));
	}
}

std::vector<std::size_t> avif_frames_to_scan(const unsigned char* avif_data, std::size_t avif_size, double threshold, std::size_t* total_frames)
{
	avifDecoder* decoder = open_avif_decoder(avif_data, avif_size);

	try {
		cv::Ptr<cv::img_hash::PHash> hasher = cv::img_hash::PHash::create();
		cv::Mat previous_hash;
		std::vector<std::size_t> frames;
		std::vector<unsigned char> rgba;
		std::size_t frame_index = 0;

		while (true) {
			const avifResult result = avifDecoderNextImage(decoder);

			if (result == AVIF_RESULT_NO_IMAGES_REMAINING) {
				break;
			}

			if (result != AVIF_RESULT_OK) {
				throw std::runtime_error("avif_frame_decode_failed: " + avif_error(result));
			}

			int width = 0;
			int height = 0;
			avif_frame_to_rgba(decoder, rgba, width, height);

			cv::Mat image(height, width, CV_8UC4, rgba.data(), static_cast<std::size_t>(width) * 4);
			cv::Mat greyscale;
			cv::Mat current_hash;

			cv::cvtColor(image, greyscale, cv::COLOR_RGBA2GRAY);
			hasher->compute(greyscale, current_hash);

			bool scan = previous_hash.empty();

			if (!scan) {
				const double distance = hasher->compare(previous_hash, current_hash);
				scan = distance >= threshold;
			}

			if (scan) {
				frames.push_back(frame_index);
				current_hash.copyTo(previous_hash);
			}

			++frame_index;

			if (frames.size() >= max_avif_scan_frames) {
				break;
			}
		}

		if (total_frames) {
			*total_frames = frame_index;
		}

		avifDecoderDestroy(decoder);
		return frames;
	} catch (...) {
		avifDecoderDestroy(decoder);
		throw;
	}
}

void decode_avif_frames(const unsigned char* avif_data, std::size_t avif_size, const std::vector<std::size_t>& frames, const animation_frame_callback& callback)
{
	if (frames.empty()) {
		return;
	}

	avifDecoder* decoder = open_avif_decoder(avif_data, avif_size);

	try {
		std::vector<unsigned char> rgba;
		std::size_t frame_index = 0;
		std::size_t selected_index = 0;

		while (selected_index < frames.size()) {
			const avifResult result = avifDecoderNextImage(decoder);

			if (result == AVIF_RESULT_NO_IMAGES_REMAINING) {
				break;
			}

			if (result != AVIF_RESULT_OK) {
				throw std::runtime_error("avif_frame_decode_failed: " + avif_error(result));
			}

			if (frame_index == frames[selected_index]) {
				int width = 0;
				int height = 0;
				avif_frame_to_rgba(decoder, rgba, width, height);
				callback(frame_index, rgba.data(), width, height);
				++selected_index;
			}

			++frame_index;
		}

		if (selected_index != frames.size()) {
			throw std::runtime_error("AVIF ended before all selected frames were decoded");
		}

		avifDecoderDestroy(decoder);
	} catch (...) {
		avifDecoderDestroy(decoder);
		throw;
	}
}

std::string flatten_avif(const std::string& file_content)
{
	if (!is_avif(file_content)) {
		return file_content;
	}

	avifDecoder* decoder = open_avif_decoder(reinterpret_cast<const unsigned char*>(file_content.data()), file_content.size());

	try {
		const avifResult result = avifDecoderNextImage(decoder);

		if (result != AVIF_RESULT_OK) {
			throw std::runtime_error("avif_frame_decode_failed: " + avif_error(result));
		}

		std::vector<unsigned char> rgba;
		int width = 0;
		int height = 0;
		avif_frame_to_rgba(decoder, rgba, width, height);

		const std::string flattened = rgba_to_png(rgba.data(), width, height);
		avifDecoderDestroy(decoder);
		return flattened;
	} catch (...) {
		avifDecoderDestroy(decoder);
		throw;
	}
}
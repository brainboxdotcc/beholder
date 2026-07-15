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
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <webp/decode.h>
#include <webp/demux.h>
#include <opencv2/core.hpp>
#include <opencv2/img_hash.hpp>
#include <opencv2/imgproc.hpp>

constexpr std::size_t max_webp_scan_frames = 100;

bool is_webp(const std::string& file_content)
{
	if (file_content.size() < 12) {
		return false;
	}

	const auto* data = reinterpret_cast<const unsigned char*>(file_content.data());

	return data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
	       data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P';
}

bool is_animated_webp(const std::string& file_content)
{
	if (!is_webp(file_content)) {
		return false;
	}

	WebPBitstreamFeatures features{};

	if (WebPGetFeatures(reinterpret_cast<const uint8_t*>(file_content.data()), file_content.size(), &features) != VP8_STATUS_OK) {
		return false;
	}

	return features.has_animation != 0;
}

WebPAnimDecoder* open_webp_decoder(const unsigned char* webp_data, std::size_t webp_size, WebPAnimInfo& info)
{
	if (!webp_data || webp_size == 0) {
		throw std::invalid_argument("Invalid WebP data");
	}

	WebPData input{};
	input.bytes = webp_data;
	input.size = webp_size;

	WebPAnimDecoderOptions options{};

	if (!WebPAnimDecoderOptionsInit(&options)) {
		throw std::runtime_error("webp_decoder_options_init_failed");
	}

	options.color_mode = MODE_RGBA;

	WebPAnimDecoder* decoder = WebPAnimDecoderNew(&input, &options);

	if (!decoder) {
		throw std::runtime_error("webp_decoder_open_failed");
	}

	if (!WebPAnimDecoderGetInfo(decoder, &info)) {
		WebPAnimDecoderDelete(decoder);
		throw std::runtime_error("webp_decoder_info_failed");
	}

	if (info.canvas_width == 0 || info.canvas_height == 0) {
		WebPAnimDecoderDelete(decoder);
		throw std::runtime_error("invalid_webp_dimensions");
	}

	return decoder;
}

std::vector<std::size_t> webp_frames_to_scan(const unsigned char* webp_data, std::size_t webp_size, double threshold, std::size_t* total_frames)
{
	WebPAnimInfo info{};
	WebPAnimDecoder* decoder = open_webp_decoder(webp_data, webp_size, info);

	try {
		cv::Ptr<cv::img_hash::PHash> hasher = cv::img_hash::PHash::create();
		cv::Mat previous_hash;
		std::vector<std::size_t> frames;
		std::size_t frame_index = 0;

		while (WebPAnimDecoderHasMoreFrames(decoder) && frames.size() < max_webp_scan_frames) {
			uint8_t* pixels = nullptr;
			int timestamp = 0;

			if (!WebPAnimDecoderGetNext(decoder, &pixels, &timestamp) || !pixels) {
				throw std::runtime_error("webp_frame_decode_failed");
			}

			cv::Mat rgba(static_cast<int>(info.canvas_height), static_cast<int>(info.canvas_width), CV_8UC4, pixels);
			cv::Mat greyscale;
			cv::Mat current_hash;

			cv::cvtColor(rgba, greyscale, cv::COLOR_RGBA2GRAY);
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
		}

		if (total_frames) {
			*total_frames = frame_index;
		}

		WebPAnimDecoderDelete(decoder);
		return frames;
	} catch (...) {
		WebPAnimDecoderDelete(decoder);
		throw;
	}
}

void decode_webp_frames(const unsigned char* webp_data, std::size_t webp_size, const std::vector<std::size_t>& frames, const animation_frame_callback& callback)
{
	if (frames.empty()) {
		return;
	}

	WebPAnimInfo info{};
	WebPAnimDecoder* decoder = open_webp_decoder(webp_data, webp_size, info);

	try {
		std::size_t frame_index = 0;
		std::size_t selected_index = 0;

		while (selected_index < frames.size() && WebPAnimDecoderHasMoreFrames(decoder)) {
			uint8_t* pixels = nullptr;
			int timestamp = 0;

			if (!WebPAnimDecoderGetNext(decoder, &pixels, &timestamp) || !pixels) {
				throw std::runtime_error("webp_frame_decode_failed");
			}

			if (frame_index == frames[selected_index]) {
				callback(frame_index, pixels, static_cast<int>(info.canvas_width), static_cast<int>(info.canvas_height));
				++selected_index;
			}

			++frame_index;
		}

		if (selected_index != frames.size()) {
			throw std::runtime_error("WebP ended before all selected frames were decoded");
		}

		WebPAnimDecoderDelete(decoder);
	} catch (...) {
		WebPAnimDecoderDelete(decoder);
		throw;
	}
}

std::string flatten_webp(const std::string& file_content)
{
	if (!is_webp(file_content)) {
		return file_content;
	}

	WebPAnimInfo info{};
	WebPAnimDecoder* decoder = open_webp_decoder(reinterpret_cast<const unsigned char*>(file_content.data()), file_content.size(), info);

	try {
		uint8_t* pixels = nullptr;
		int timestamp = 0;

		if (!WebPAnimDecoderGetNext(decoder, &pixels, &timestamp) || !pixels) {
			throw std::runtime_error("webp_frame_decode_failed");
		}

		const std::string flattened = rgba_to_png(pixels, static_cast<int>(info.canvas_width), static_cast<int>(info.canvas_height));
		WebPAnimDecoderDelete(decoder);
		return flattened;
	} catch (...) {
		WebPAnimDecoderDelete(decoder);
		throw;
	}
}
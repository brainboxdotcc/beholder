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
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/error.h>
	#include <libavutil/imgutils.h>
	#include <libswscale/swscale.h>
}
#include <opencv2/core.hpp>
#include <opencv2/img_hash.hpp>
#include <opencv2/imgproc.hpp>

constexpr std::size_t mp4_io_buffer_size = 32768;
constexpr std::size_t max_mp4_scan_frames = 100;

struct mp4_memory_buffer {
	const unsigned char* data{nullptr};
	std::size_t size{0};
	std::size_t position{0};
};

std::string ffmpeg_error(int error)
{
	char buffer[AV_ERROR_MAX_STRING_SIZE]{};

	if (av_strerror(error, buffer, sizeof(buffer)) != 0) {
		return "unknown FFmpeg error";
	}

	return buffer;
}

int mp4_read(void* opaque, uint8_t* buffer, int buffer_size)
{
	auto* input = static_cast<mp4_memory_buffer*>(opaque);

	if (!input || !buffer || buffer_size <= 0) {
		return AVERROR(EINVAL);
	}

	if (input->position >= input->size) {
		return AVERROR_EOF;
	}

	const std::size_t available = input->size - input->position;
	const std::size_t amount = std::min<std::size_t>(available, static_cast<std::size_t>(buffer_size));

	std::memcpy(buffer, input->data + input->position, amount);
	input->position += amount;

	return static_cast<int>(amount);
}

int64_t mp4_seek(void* opaque, int64_t offset, int whence)
{
	auto* input = static_cast<mp4_memory_buffer*>(opaque);

	if (!input) {
		return AVERROR(EINVAL);
	}

	if (whence == AVSEEK_SIZE) {
		if (input->size > static_cast<std::size_t>(std::numeric_limits<int64_t>::max())) {
			return AVERROR(EOVERFLOW);
		}

		return static_cast<int64_t>(input->size);
	}

	const int origin = whence & ~AVSEEK_FORCE;
	int64_t base = 0;

	if (origin == SEEK_SET) {
		base = 0;
	} else if (origin == SEEK_CUR) {
		base = static_cast<int64_t>(input->position);
	} else if (origin == SEEK_END) {
		base = static_cast<int64_t>(input->size);
	} else {
		return AVERROR(EINVAL);
	}

	if ((offset > 0 && base > std::numeric_limits<int64_t>::max() - offset) ||
	    (offset < 0 && base < std::numeric_limits<int64_t>::min() - offset)) {
		return AVERROR(EOVERFLOW);
	}

	const int64_t position = base + offset;

	if (position < 0 || static_cast<uint64_t>(position) > input->size) {
		return AVERROR(EINVAL);
	}

	input->position = static_cast<std::size_t>(position);
	return position;
}

struct mp4_decoder {
	mp4_memory_buffer input;
	AVFormatContext* format{nullptr};
	AVIOContext* io{nullptr};
	AVCodecContext* codec{nullptr};
	AVPacket* packet{nullptr};
	AVFrame* frame{nullptr};
	SwsContext* scaler{nullptr};
	int video_stream{-1};
	std::vector<unsigned char> rgba;
};

void close_mp4_decoder(mp4_decoder& decoder)
{
	sws_freeContext(decoder.scaler);
	decoder.scaler = nullptr;

	av_frame_free(&decoder.frame);
	av_packet_free(&decoder.packet);
	avcodec_free_context(&decoder.codec);

	if (decoder.format) {
		avformat_close_input(&decoder.format);
	}

	if (decoder.io) {
		av_freep(&decoder.io->buffer);
		avio_context_free(&decoder.io);
	}
}

void open_mp4_decoder(mp4_decoder& decoder, const unsigned char* mp4_data, std::size_t mp4_size)
{
	if (!mp4_data || mp4_size == 0) {
		throw std::invalid_argument("Invalid MP4 data");
	}

	decoder.input.data = mp4_data;
	decoder.input.size = mp4_size;

	decoder.format = avformat_alloc_context();

	if (!decoder.format) {
		throw std::runtime_error("avformat_alloc_context_failed");
	}

	unsigned char* io_buffer = static_cast<unsigned char*>(av_malloc(mp4_io_buffer_size));

	if (!io_buffer) {
		close_mp4_decoder(decoder);
		throw std::runtime_error("avio_buffer_alloc_failed");
	}

	decoder.io = avio_alloc_context(
		io_buffer,
		mp4_io_buffer_size,
		0,
		&decoder.input,
		mp4_read,
		nullptr,
		mp4_seek
	);

	if (!decoder.io) {
		av_free(io_buffer);
		close_mp4_decoder(decoder);
		throw std::runtime_error("avio_alloc_context_failed");
	}

	decoder.format->pb = decoder.io;
	decoder.format->flags |= AVFMT_FLAG_CUSTOM_IO;

	int result = avformat_open_input(&decoder.format, nullptr, nullptr, nullptr);

	if (result < 0) {
		const std::string error = ffmpeg_error(result);
		close_mp4_decoder(decoder);
		throw std::runtime_error("avformat_open_input_failed: " + error);
	}

	result = avformat_find_stream_info(decoder.format, nullptr);

	if (result < 0) {
		const std::string error = ffmpeg_error(result);
		close_mp4_decoder(decoder);
		throw std::runtime_error("avformat_find_stream_info_failed: " + error);
	}

	decoder.video_stream = av_find_best_stream(decoder.format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

	if (decoder.video_stream < 0) {
		const std::string error = ffmpeg_error(decoder.video_stream);
		close_mp4_decoder(decoder);
		throw std::runtime_error("video_stream_not_found: " + error);
	}

	AVStream* stream = decoder.format->streams[decoder.video_stream];
	const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);

	if (!codec) {
		close_mp4_decoder(decoder);
		throw std::runtime_error("video_decoder_not_found");
	}

	decoder.codec = avcodec_alloc_context3(codec);

	if (!decoder.codec) {
		close_mp4_decoder(decoder);
		throw std::runtime_error("avcodec_alloc_context_failed");
	}

	result = avcodec_parameters_to_context(decoder.codec, stream->codecpar);

	if (result < 0) {
		const std::string error = ffmpeg_error(result);
		close_mp4_decoder(decoder);
		throw std::runtime_error("avcodec_parameters_to_context_failed: " + error);
	}

	result = avcodec_open2(decoder.codec, codec, nullptr);

	if (result < 0) {
		const std::string error = ffmpeg_error(result);
		close_mp4_decoder(decoder);
		throw std::runtime_error("avcodec_open_failed: " + error);
	}

	decoder.packet = av_packet_alloc();
	decoder.frame = av_frame_alloc();

	if (!decoder.packet || !decoder.frame) {
		close_mp4_decoder(decoder);
		throw std::runtime_error("video_frame_alloc_failed");
	}
}

const unsigned char* mp4_frame_rgba(mp4_decoder& decoder, int& width, int& height)
{
	width = decoder.frame->width;
	height = decoder.frame->height;

	if (width <= 0 || height <= 0) {
		throw std::runtime_error("invalid_video_frame_dimensions");
	}

	const std::size_t row_size = static_cast<std::size_t>(width) * 4;
	const std::size_t buffer_size = row_size * static_cast<std::size_t>(height);

	if (row_size / 4 != static_cast<std::size_t>(width) || (height > 0 && buffer_size / static_cast<std::size_t>(height) != row_size)) {
		throw std::runtime_error("video_frame_size_overflow");
	}

	decoder.rgba.resize(buffer_size);

	decoder.scaler = sws_getCachedContext(
		decoder.scaler,
		width,
		height,
		static_cast<AVPixelFormat>(decoder.frame->format),
		width,
		height,
		AV_PIX_FMT_RGBA,
		SWS_BILINEAR,
		nullptr,
		nullptr,
		nullptr
	);

	if (!decoder.scaler) {
		throw std::runtime_error("sws_get_cached_context_failed");
	}

	uint8_t* destination[] = {
		decoder.rgba.data(),
		nullptr,
		nullptr,
		nullptr
	};

	int destination_stride[] = {
		width * 4,
		0,
		0,
		0
	};

	const int result = sws_scale(
		decoder.scaler,
		decoder.frame->data,
		decoder.frame->linesize,
		0,
		height,
		destination,
		destination_stride
	);

	if (result != height) {
		throw std::runtime_error("sws_scale_failed");
	}

	return decoder.rgba.data();
}

template<typename FrameCallback>
void read_mp4_frames(mp4_decoder& decoder, const FrameCallback& callback)
{
	auto receive_frames = [&decoder, &callback]() {
		while (true) {
			const int result = avcodec_receive_frame(decoder.codec, decoder.frame);

			if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
				return true;
			}

			if (result < 0) {
				throw std::runtime_error("avcodec_receive_frame_failed: " + ffmpeg_error(result));
			}

			int width = 0;
			int height = 0;
			const unsigned char* pixels = mp4_frame_rgba(decoder, width, height);

			if (!callback(pixels, width, height)) {
				av_frame_unref(decoder.frame);
				return false;
			}

			av_frame_unref(decoder.frame);
		}
	};

	while (av_read_frame(decoder.format, decoder.packet) >= 0) {
		if (decoder.packet->stream_index == decoder.video_stream) {
			const int result = avcodec_send_packet(decoder.codec, decoder.packet);

			if (result < 0 && result != AVERROR(EAGAIN)) {
				av_packet_unref(decoder.packet);
				throw std::runtime_error("avcodec_send_packet_failed: " + ffmpeg_error(result));
			}

			if (!receive_frames()) {
				av_packet_unref(decoder.packet);
				return;
			}
		}

		av_packet_unref(decoder.packet);
	}

	const int result = avcodec_send_packet(decoder.codec, nullptr);

	if (result < 0 && result != AVERROR_EOF) {
		throw std::runtime_error("avcodec_flush_failed: " + ffmpeg_error(result));
	}

	receive_frames();
}

std::vector<std::size_t> mp4_frames_to_scan(const unsigned char* mp4_data, std::size_t mp4_size, double threshold, std::size_t* total_frames)
{
	mp4_decoder decoder;

	try {
		open_mp4_decoder(decoder, mp4_data, mp4_size);

		cv::Ptr<cv::img_hash::PHash> hasher = cv::img_hash::PHash::create();
		cv::Mat previous_hash;
		std::vector<std::size_t> frames;
		std::size_t frame_index = 0;

		read_mp4_frames(
			decoder,
			[&](const unsigned char* pixels, int width, int height) {
				cv::Mat rgba(height, width, CV_8UC4, const_cast<unsigned char*>(pixels));
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

				return frames.size() < max_mp4_scan_frames;
			}
		);

		if (total_frames) {
			*total_frames = frame_index;
		}

		close_mp4_decoder(decoder);
		return frames;
	} catch (...) {
		close_mp4_decoder(decoder);
		throw;
	}
}

void decode_mp4_frames(const unsigned char* mp4_data, std::size_t mp4_size, const std::vector<std::size_t>& frames, const gif_frame_callback& callback)
{
	if (frames.empty()) {
		return;
	}

	mp4_decoder decoder;

	try {
		open_mp4_decoder(decoder, mp4_data, mp4_size);

		std::size_t frame_index = 0;
		std::size_t selected_index = 0;

		read_mp4_frames(
			decoder,
			[&](const unsigned char* pixels, int width, int height) {
				if (frame_index == frames[selected_index]) {
					callback(frame_index, pixels, width, height);
					++selected_index;
				}

				++frame_index;
				return selected_index < frames.size();
			}
		);

		if (selected_index != frames.size()) {
			throw std::runtime_error("MP4 ended before all selected frames were decoded");
		}

		close_mp4_decoder(decoder);
	} catch (...) {
		close_mp4_decoder(decoder);
		throw;
	}
}
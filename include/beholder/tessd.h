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

	/**
	 * @brief Process exit codes used by tessd.
	 *
	 * These values are returned to the parent process to indicate why the
	 * worker terminated. A value of no_error indicates successful completion.
	 */
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

	/**
	 * @brief Human-readable descriptions for tessd exit codes.
	 *
	 * Indexed using the integer value of an exit_code enumeration.
	 */
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

	/**
	 * @brief Terminate the current process with the specified exit code.
	 *
	 * @param e Exit status to return to the parent process.
	 */
	inline void status(exit_code e) {
		exit(static_cast<int>(e));
	}

};

/**
 * @brief Callback invoked for each decoded GIF or MP4 frame.
 *
 * The callback receives the fully composited frame in 32-bit RGBA format.
 * The pixel buffer remains owned by the decoder and is only valid for the
 * lifetime of the callback.
 */
using animation_frame_callback = std::function<void(std::size_t, const unsigned char*, int, int)>;

/**
 * @brief Select perceptually distinct frames from an animated GIF.
 *
 * Decodes every frame of the GIF and compares each composited frame against
 * the previously selected frame using a perceptual hash. Frames whose hash
 * distance exceeds the supplied threshold are returned.
 *
 * @param gif_data Pointer to GIF file data.
 * @param gif_size Size of GIF data in bytes.
 * @param threshold Minimum perceptual hash distance required to select a frame.
 * @param total_frames Optional pointer receiving the total number of frames.
 * @return Vector of selected frame indices.
 */
std::vector<std::size_t> gif_frames_to_scan(const unsigned char* gif_data, std::size_t gif_size, double threshold = 6.0, std::size_t* total_frames = nullptr);

/**
 * @brief Decode selected frames from an animated GIF.
 *
 * Only the requested frame indices are passed to the callback. Frames are
 * presented as fully composited 32-bit RGBA images.
 *
 * @param gif_data Pointer to GIF file data.
 * @param gif_size Size of GIF data in bytes.
 * @param frames Frame indices to decode.
 * @param callback Callback invoked for each decoded frame.
 */
void decode_gif_frames(const unsigned char* gif_data, std::size_t gif_size, const std::vector<std::size_t>& frames, const animation_frame_callback& callback);

/**
 * @brief Execute tessd in command line test mode.
 *
 * @param argc Command line argument count.
 * @param argv Command line argument vector.
 * @return Process exit status.
 */
int tessd_cli(int argc, char** argv);

/**
 * @brief Perform OCR across selected GIF frames.
 *
 * @param file_content GIF file data.
 * @param frames Frame indices to scan.
 * @return Concatenated OCR output.
 */
std::string run_tesseract_gif(const std::string& file_content, const std::vector<std::size_t>& frames, const std::string& languages);

/**
 * @brief Perform NSFW classification across selected GIF frames.
 *
 * The highest score for each category is returned.
 *
 * @param file_content GIF file data.
 * @param frames Frame indices to scan.
 * @return NSFW classification result.
 */
dpp::json run_basic_nsfw_gif(const std::string& file_content, const std::vector<std::size_t>& frames);

/**
 * @brief Perform OCR across selected MP4 frames.
 *
 * @param file_content MP4 file data.
 * @param frames Frame indices to scan.
 * @return Concatenated OCR output.
 */
std::string run_tesseract_mp4(const std::string& file_content, const std::vector<std::size_t>& frames, const std::string& languages);

/**
 * @brief Perform NSFW classification across selected MP4 frames.
 *
 * The highest score for each category is returned.
 *
 * @param file_content MP4 file data.
 * @param frames Frame indices to scan.
 * @return NSFW classification result.
 */
dpp::json run_basic_nsfw_mp4(const std::string& file_content, const std::vector<std::size_t>& frames);

/**
 * @brief Select perceptually distinct frames from an MP4 video.
 *
 * Frames are decoded using FFmpeg and compared using a perceptual hash.
 *
 * @param mp4_data Pointer to MP4 file data.
 * @param mp4_size Size of MP4 data in bytes.
 * @param threshold Minimum perceptual hash distance required to select a frame.
 * @param total_frames Optional pointer receiving the total number of decoded frames.
 * @return Vector of selected frame indices.
 */
std::vector<std::size_t> mp4_frames_to_scan(const unsigned char* mp4_data, std::size_t mp4_size, double threshold = 12.0, std::size_t* total_frames = nullptr);

/**
 * @brief Decode selected frames from an MP4 video.
 *
 * Frames are supplied to the callback as 32-bit RGBA images.
 *
 * @param mp4_data Pointer to MP4 file data.
 * @param mp4_size Size of MP4 data in bytes.
 * @param frames Frame indices to decode.
 * @param callback Callback invoked for each decoded frame.
 */
void decode_mp4_frames(const unsigned char* mp4_data, std::size_t mp4_size, const std::vector<std::size_t>& frames, const animation_frame_callback& callback);

/**
 * @brief Determine whether a file is an MP4 video.
 *
 * @param file_content File data.
 * @return True if the file appears to be an MP4.
 */
bool is_mp4(const std::string& file_content);

/**
 * @brief Flatten an animated GIF to its first frame.
 *
 * Non-animated images are returned unchanged.
 *
 * @param filename Original filename.
 * @param file_content Image data.
 * @return PNG image data containing the first frame.
 */
std::string flatten_gif(const std::string& filename, const std::string& file_content);

/**
 * @brief Encode an RGBA image as a PNG.
 *
 * @param pixels Pointer to RGBA pixel data.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @return PNG image data.
 */
std::string rgba_to_png(const unsigned char* pixels, int width, int height);

/**
 * @brief Determine whether a GIF is animated.
 *
 * @param file_content GIF file data.
 * @return True if the GIF contains animation control blocks.
 */
bool is_animated_gif(const std::string& file_content);

/**
 * @brief Determine whether file data contains a WebP image.
 *
 * @param file_content File data.
 * @return True if the file has a valid WebP container signature.
 */
bool is_webp(const std::string& file_content);

/**
 * @brief Determine whether a WebP image is animated.
 *
 * @param file_content WebP file data.
 * @return True if the WebP bitstream contains animation.
 */
bool is_animated_webp(const std::string& file_content);

/**
 * @brief Select perceptually distinct frames from an animated WebP image.
 *
 * @param webp_data Pointer to WebP file data.
 * @param webp_size Size of WebP data in bytes.
 * @param threshold Minimum perceptual hash distance required to select a frame.
 * @param total_frames Optional pointer receiving the total number of decoded frames.
 * @return Vector of selected frame indices.
 */
std::vector<std::size_t> webp_frames_to_scan(const unsigned char* webp_data, std::size_t webp_size, double threshold = 6.0, std::size_t* total_frames = nullptr);

/**
 * @brief Decode selected frames from an animated WebP image.
 *
 * Frames are supplied to the callback as complete composited 32-bit RGBA images.
 *
 * @param webp_data Pointer to WebP file data.
 * @param webp_size Size of WebP data in bytes.
 * @param frames Frame indices to decode.
 * @param callback Callback invoked for each decoded frame.
 */
void decode_webp_frames(const unsigned char* webp_data, std::size_t webp_size, const std::vector<std::size_t>& frames, const animation_frame_callback& callback);

/**
 * @brief Flatten a WebP image to its first frame.
 *
 * Static and animated WebP images are converted to PNG so they can pass through
 * the existing static image scanning pipeline.
 *
 * @param file_content WebP file data.
 * @return PNG image data containing the first frame.
 */
std::string flatten_webp(const std::string& file_content);

/**
 * @brief Perform OCR across selected WebP frames.
 *
 * @param file_content WebP file data.
 * @param frames Frame indices to scan.
 * @return Concatenated OCR output.
 */
std::string run_tesseract_webp(const std::string& file_content, const std::vector<std::size_t>& frames, const std::string& languages);

/**
 * @brief Perform NSFW classification across selected WebP frames.
 *
 * The highest score for each category is returned.
 *
 * @param file_content WebP file data.
 * @param frames Frame indices to scan.
 * @return NSFW classification result.
 */
dpp::json run_basic_nsfw_webp(const std::string& file_content, const std::vector<std::size_t>& frames);

/**
 * @brief Determine whether file data contains an AVIF image.
 *
 * @param file_content File data.
 * @return True if the file contains an AVIF-compatible file type.
 */
bool is_avif(const std::string& file_content);

/**
 * @brief Determine whether an AVIF image contains multiple frames.
 *
 * @param file_content AVIF file data.
 * @return True if the AVIF contains an image sequence.
 */
bool is_animated_avif(const std::string& file_content);

/**
 * @brief Select perceptually distinct frames from an animated AVIF image.
 *
 * @param avif_data Pointer to AVIF file data.
 * @param avif_size Size of AVIF data in bytes.
 * @param threshold Minimum perceptual hash distance required to select a frame.
 * @param total_frames Optional pointer receiving the total number of decoded frames.
 * @return Vector of selected frame indices.
 */
std::vector<std::size_t> avif_frames_to_scan(const unsigned char* avif_data, std::size_t avif_size, double threshold = 6.0, std::size_t* total_frames = nullptr);

/**
 * @brief Decode selected frames from an AVIF image sequence.
 *
 * Frames are supplied to the callback as tightly packed 32-bit RGBA images.
 *
 * @param avif_data Pointer to AVIF file data.
 * @param avif_size Size of AVIF data in bytes.
 * @param frames Frame indices to decode.
 * @param callback Callback invoked for each decoded frame.
 */
void decode_avif_frames(const unsigned char* avif_data, std::size_t avif_size, const std::vector<std::size_t>& frames, const animation_frame_callback& callback);

/**
 * @brief Flatten an AVIF image to its first frame.
 *
 * @param file_content AVIF file data.
 * @return PNG image data containing the first decoded frame.
 */
std::string flatten_avif(const std::string& file_content);

/**
 * @brief Perform OCR across selected AVIF frames.
 *
 * @param file_content AVIF file data.
 * @param frames Frame indices to scan.
 * @return Concatenated OCR output.
 */
std::string run_tesseract_avif(const std::string& file_content, const std::vector<std::size_t>& frames, const std::string& languages);

/**
 * @brief Perform NSFW classification across selected AVIF frames.
 *
 * @param file_content AVIF file data.
 * @param frames Frame indices to scan.
 * @return NSFW classification result.
 */
dpp::json run_basic_nsfw_avif(const std::string& file_content, const std::vector<std::size_t>& frames);

bool is_webm(const std::string& file_content);

bool run_profanity_filter(const std::string& text, const std::vector<std::string>& languages);


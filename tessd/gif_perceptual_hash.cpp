#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <nsfwd/stb_image.h>
#include <opencv2/core.hpp>
#include <opencv2/img_hash.hpp>
#include <opencv2/imgproc.hpp>

/*
 * This must be compiled in the same translation unit as STB_IMAGE_IMPLEMENTATION
 * because it deliberately uses stb_image's internal GIF decoder API.
 */
std::vector<std::size_t> gif_frames_to_scan(const unsigned char* gif_data, std::size_t gif_size, double threshold = 8.0)
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

		if (!pixels) {
			break;
		}

		if (gif_state.w <= 0 || gif_state.h <= 0) {
			break;
		}

		/*
		 * stbi__gif_load_next() returns gif_state.out, which is the complete
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
		}

		++frame_index;
	}

	STBI_FREE(gif_state.out);
	STBI_FREE(gif_state.history);
	STBI_FREE(gif_state.background);

	return frames;
}


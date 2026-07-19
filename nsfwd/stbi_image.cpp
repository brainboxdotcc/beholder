#include <nsfwd/stbi_image.h>
#define STB_IMAGE_IMPLEMENTATION
#include <nsfwd/stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <nsfwd/stb_image_resize2.h>
#include <nsfwd/nsfwd.h>
#include <emmintrin.h>
#include <avif/avif.h>
#include <webp/decode.h>

stbi_image::stbi_image(std::string_view body) {
	const auto *data = reinterpret_cast<const stbi_uc *>(body.data());
	const size_t size = body.size();

	image = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, INPUT_CHANNELS);
	if (image) {
		channels = INPUT_CHANNELS;
		return;
	}

	int webp_width = 0;
	int webp_height = 0;

	if (WebPGetInfo(data, size, &webp_width, &webp_height) && webp_width > 0 && webp_height > 0 && static_cast<size_t>(webp_width) <= SIZE_MAX / static_cast<size_t>(webp_height) / INPUT_CHANNELS) {
		const size_t output_size = static_cast<size_t>(webp_width) * static_cast<size_t>(webp_height) * INPUT_CHANNELS;
		image = static_cast<stbi_uc *>(STBI_MALLOC(output_size));

		if (image && WebPDecodeRGBInto(data, size, image, output_size, webp_width * INPUT_CHANNELS)) {
			width = webp_width;
			height = webp_height;
			channels = INPUT_CHANNELS;
			return;
		}

		STBI_FREE(image);
		image = nullptr;
	}

	avifDecoder *decoder = avifDecoderCreate();

	if (!decoder) {
		return;
	}

	avifResult result = avifDecoderSetIOMemory(decoder, data, size);

	if (result == AVIF_RESULT_OK) {
		result = avifDecoderParse(decoder);
	}

	if (result == AVIF_RESULT_OK) {
		result = avifDecoderNextImage(decoder);
	}

	if (result == AVIF_RESULT_OK && decoder->image->width > 0 && decoder->image->height > 0 && static_cast<size_t>(decoder->image->width) <= SIZE_MAX / static_cast<size_t>(decoder->image->height) / INPUT_CHANNELS) {
		const size_t output_size = static_cast<size_t>(decoder->image->width) * static_cast<size_t>(decoder->image->height) * INPUT_CHANNELS;
		image = static_cast<stbi_uc *>(STBI_MALLOC(output_size));

		if (image) {
			avifRGBImage rgb;
			avifRGBImageSetDefaults(&rgb, decoder->image);
			rgb.format = AVIF_RGB_FORMAT_RGB;
			rgb.depth = 8;
			rgb.pixels = image;
			rgb.rowBytes = decoder->image->width * INPUT_CHANNELS;
			result = avifImageYUVToRGB(decoder->image, &rgb);

			if (result == AVIF_RESULT_OK) {
				width = static_cast<int>(decoder->image->width);
				height = static_cast<int>(decoder->image->height);
				channels = INPUT_CHANNELS;
			} else {
				STBI_FREE(image);
				image = nullptr;
			}
		}
	}

	avifDecoderDestroy(decoder);
}

stbi_image::~stbi_image() {
	stbi_image_free(image);
}

stbi_image::operator bool() const {
	return image != nullptr;
}

int stbi_image::get_width() const {
	return width;
}

int stbi_image::get_height() const {
	return height;
}

int stbi_image::get_channels() const {
	return channels;
}

void stbi_image::resize_and_normalise(float *dest) const {
	alignas(16) static thread_local unsigned char resized[INPUT_SIZE_SSE];
	stbir_resize_uint8_linear(image, width, height, 0, resized, INPUT_WIDTH, INPUT_HEIGHT, 0, STBIR_RGB);

	const __m128 scale = _mm_set1_ps(1.0f / 255.0f);

	for (size_t i = 0; i < INPUT_SIZE_SSE; i += 16) {
		__m128i bytes = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&resized[i]));
		__m128i zero = _mm_setzero_si128();
		__m128i lo16 = _mm_unpacklo_epi8(bytes, zero);
		__m128i hi16 = _mm_unpackhi_epi8(bytes, zero);
		__m128i lo32a = _mm_unpacklo_epi16(lo16, zero);
		__m128i lo32b = _mm_unpackhi_epi16(lo16, zero);
		__m128i hi32a = _mm_unpacklo_epi16(hi16, zero);
		__m128i hi32b = _mm_unpackhi_epi16(hi16, zero);

		_mm_storeu_ps(&dest[i], _mm_mul_ps(_mm_cvtepi32_ps(lo32a), scale));
		_mm_storeu_ps(&dest[i + 4], _mm_mul_ps(_mm_cvtepi32_ps(lo32b), scale));
		_mm_storeu_ps(&dest[i + 8], _mm_mul_ps(_mm_cvtepi32_ps(hi32a), scale));
		_mm_storeu_ps(&dest[i + 12], _mm_mul_ps(_mm_cvtepi32_ps(hi32b), scale));
	}
}

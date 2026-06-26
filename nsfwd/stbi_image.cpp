#include <nsfwd/stbi_image.h>
#define STB_IMAGE_IMPLEMENTATION
#include <nsfwd/stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <nsfwd/stb_image_resize2.h>
#include <nsfwd/nsfwd.h>
#include <emmintrin.h>

stbi_image::stbi_image(std::string_view body) {
	image = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(body.data()), body.size(), &width, &height, &channels, INPUT_CHANNELS);
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

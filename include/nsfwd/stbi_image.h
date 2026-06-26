#pragma once
#include <string_view>

class stbi_image {
public:
	stbi_image(std::string_view body);

	~stbi_image();

	stbi_image(const stbi_image&) = delete;
	stbi_image& operator=(const stbi_image&) = delete;
	stbi_image(stbi_image&&) = delete;
	stbi_image& operator=(stbi_image&&) = delete;

	operator bool() const;

	int get_width() const;

	int get_height() const;

	int get_channels() const;

	void resize_and_normalise(float *dest) const;

private:
	unsigned char *image = nullptr;
	int width = 0;
	int height = 0;
	int channels = 0;
};
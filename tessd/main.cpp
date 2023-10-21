#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <iostream>
#include <array>

int main(int argc, char const *argv[])
{
	std::freopen(nullptr, "rb", stdin);
	fclose(stderr);

        std::size_t len;
	const std::size_t INIT_BUFFER_SIZE = 10240;
        std::array<char, INIT_BUFFER_SIZE> buf;

        // somewhere to store the data
        std::vector<char> input;

        // use std::fread and remember to only use as many bytes as are returned
        // according to len
        while((len = std::fread(buf.data(), sizeof(buf[0]), buf.size(), stdin)) > 0)
        {
            // whoopsie
            if(std::ferror(stdin) && !std::feof(stdin))
                throw std::runtime_error(std::strerror(errno));

            // use {buf.data(), buf.data() + len} here
            input.insert(input.end(), buf.data(), buf.data() + len); // append to vector
        }
	
	tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();
	if (api->Init(NULL, "eng", tesseract::OcrEngineMode::OEM_DEFAULT)) {
		delete api;
		exit(1);
	}

	api->SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_BLOCK);
	Pix* image = pixReadMem((l_uint8*)input.data(), input.size());
	if (!image) {
		delete api;
		exit(1);
	}
	/* We may have already checked this value if discord gave it us as attachment metadata.
	* Just to be sure, and also in case we're processing an image given in a raw url, we check the
	* width and height again here.
	*/
	if (image->w * image->h > 33554432) {
		delete api;
		pixDestroy(&image);
		exit(1);
	}
	api->SetImage(image);
	const char* output = api->GetUTF8Text();
	pixDestroy(&image);
	api->Clear();
	if (!output) {
		exit(1);
	}

	std::cout << output << std::endl;
}
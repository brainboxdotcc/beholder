#include <dpp/dpp.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <yeet/yeet.h>

extern json configdocument;
extern std::atomic<int> concurrent_images;

void ocr_image(std::string file_content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
	tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
	if (api->Init(NULL, "eng", tesseract::OcrEngineMode::OEM_DEFAULT)) {
		bot.log(dpp::ll_error, "Could not initialise tesseract");
		concurrent_images--;
		return;
	}
	api->SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_BLOCK);
	Pix* image = pixReadMem((l_uint8*)file_content.data(), file_content.length());
	if (!image) {
		bot.log(dpp::ll_error, "Could not read image with pixRead");
		concurrent_images--;
		return;
	}
	/* We may have already checked this value if discord gave it us as attachment metadata.
	 * Just to be sure, and also in case we're processing an image given in a raw url, we check the
	 * width and height again here.
	 */
	if (image->w > 4096 || image->h > 4096) {
		bot.log(dpp::ll_info, "Image dimensions of " + std::to_string(image->w) + "x" + std::to_string(image->h) + " too large to be a screenshot");
		concurrent_images--;
		return;
	}
	image = pixConvertRGBToGray(image, 0.5, 0.3, 0.2);
	api->SetImage(image);
	const char* output = api->GetUTF8Text();
	if (!output) {
		bot.log(dpp::ll_error, "GetUTF8Text() returned nullptr!!!");
		concurrent_images--;
		return;
	}
	std::vector<std::string> lines = dpp::utility::tokenize(output, "\n");
	bot.log(dpp::ll_debug, "Read " + std::to_string(lines.size()) + " lines of text from image with total size " + std::to_string(strlen(output)));
	for (const std::string& line : lines) {
		if (line.length() && line[0] == 0x0C) {
			/* Tesseract puts random formdeeds in the output, skip them */
			continue;
		}
		bot.log(dpp::ll_info, "Image content: " + line);
		for (const std::string& pattern : configdocument.at("patterns")) {
			std::string pattern_wild = "*" + pattern + "*";
			if (line.length() && pattern.length() && match(line.c_str(), pattern_wild.c_str())) {
				concurrent_images--;
				delete_message_and_warn(bot, ev, attach, pattern);
				return;
			}
		}
	}
}

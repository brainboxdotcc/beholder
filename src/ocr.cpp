#include <dpp/dpp.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <beholder/beholder.h>
#include <beholder/database.h>

extern std::atomic<int> concurrent_images;

void ocr_image(std::string file_content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
	tesseract::TessBaseAPI api;
	if (api.Init(NULL, "eng", tesseract::OcrEngineMode::OEM_DEFAULT)) {
		bot.log(dpp::ll_error, "Could not initialise tesseract");
		concurrent_images--;
		return;
	}
	api.SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_BLOCK);
	Pix* image = pixReadMem((l_uint8*)file_content.data(), file_content.length());
	if (!image) {
		bot.log(dpp::ll_error, "Could not read image with pixRead");
		pixFreeData(image);
		concurrent_images--;
		return;
	}
	/* We may have already checked this value if discord gave it us as attachment metadata.
	 * Just to be sure, and also in case we're processing an image given in a raw url, we check the
	 * width and height again here.
	 */
	if (image->w > 4096 || image->h > 4096) {
		bot.log(dpp::ll_info, "Image dimensions of " + std::to_string(image->w) + "x" + std::to_string(image->h) + " too large to be a screenshot");
		pixFreeData(image);
		concurrent_images--;
		return;
	}
	image = pixConvertRGBToGray(image, 0.5, 0.3, 0.2);
	api.SetImage(image);
	const char* output = api.GetUTF8Text();
	pixFreeData(image);
	if (!output) {
		bot.log(dpp::ll_error, "GetUTF8Text() returned nullptr!!!");
		concurrent_images--;
		return;
	}
	std::vector<std::string> lines = dpp::utility::tokenize(output, "\n");
	delete[] output;
	bot.log(dpp::ll_debug, "Read " + std::to_string(lines.size()) + " lines of text from image with total size " + std::to_string(strlen(output)));
	db::resultset patterns = db::query("SELECT * FROM guild_patterns WHERE guild_id = '?'", { ev.msg.guild_id.str() });
	bot.log(dpp::ll_debug, "Checking image content against " + std::to_string(patterns.size()) + " patterns...");
	for (const std::string& line : lines) {
		if (line.length() && line[0] == 0x0C) {
			/* Tesseract puts random formdeeds in the output, skip them */
			continue;
		}
		bot.log(dpp::ll_info, "Image content: " + line);
		for (const db::row& pattern : patterns) {
			const std::string& p = pattern.at("pattern");
			std::string pattern_wild = "*" + p + "*";
			if (line.length() && p.length() && match(line.c_str(), pattern_wild.c_str())) {
				concurrent_images--;
				delete_message_and_warn(bot, ev, attach, p, false);
				return;
			}
		}
	}
}

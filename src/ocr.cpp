#include <dpp/dpp.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <beholder/beholder.h>
#include <beholder/database.h>

extern std::atomic<int> concurrent_images;
extern json configdocument;

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
		concurrent_images--;
		return;
	}
	/* We may have already checked this value if discord gave it us as attachment metadata.
	 * Just to be sure, and also in case we're processing an image given in a raw url, we check the
	 * width and height again here.
	 */
	if (image->w * image->h > 33554432) {
		bot.log(dpp::ll_info, "Image dimensions of " + std::to_string(image->w) + "x" + std::to_string(image->h) + " too large to be a screenshot");
		pixDestroy(&image);
		concurrent_images--;
		return;
	}
	image = pixConvertRGBToGrayFast(image);
	api.SetImage(image);
	const char* output = api.GetUTF8Text();
	pixDestroy(&image);
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

	db::resultset settings = db::query("SELECT premium_subscription FROM guild_config WHERE guild_id = ? AND calls_this_month <= calls_limit", { ev.msg.guild_id.str() });
	if (settings.size() && settings[0].at("premium_subscription").length()) {
		std::vector<std::string> fields = configdocument["ir"]["fields"];
		std::string endpoint = configdocument["ir"]["endpoint"];
		/* Only enable models the user has in their block list, to save on resources */
		db::resultset m = db::query("SELECT GROUP_CONCAT(DISTINCT model) AS selected FROM premium_filter_model WHERE category IN (SELECT pattern FROM premium_filters WHERE guild_id = ?)", { ev.msg.guild_id.str() });
		std::string active_models = m[0].at("selected");
		std::string url = endpoint
			+ "?" + fields[0] + "=" + dpp::utility::url_encode(active_models)
			+ "&" + fields[1] + "=" + dpp::utility::url_encode(configdocument["ir"]["credentials"]["username"])
			+ "&" + fields[2] + "=" + dpp::utility::url_encode(configdocument["ir"]["credentials"]["password"])
			+ "&" + fields[3] + "=" + dpp::utility::url_encode(attach.url);
		db::query("UPDATE guild_config SET calls_this_month = calls_this_month + 1 WHERE guild_id = ?", {ev.msg.guild_id.str() });
		bot.request(url, dpp::m_get,
			[attach, ev, &bot, url](const dpp::http_request_completion_t& premium_api) {
				if (premium_api.status == 200 && !premium_api.body.empty()) {
					json answer = json::parse(premium_api.body);
					find_banned_type(answer, attach, bot, ev);
				} else {
					bot.log(dpp::ll_debug, "API Error: '" + premium_api.body + "' status: " + std::to_string(premium_api.status));
				}
			}
		,"", "text/json", {{"Accept", "*/*"}}, "1.0");
	}
}

#include <dpp/dpp.h>
#include <beholder/config.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include "3rdparty/EasyGifReader.h"
void ocr_image(std::string file_content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
	tesseract::TessBaseAPI api;
	std::string ocr;

	on_thread_exit([&bot]() {
		concurrent_images--;
		bot.log(dpp::ll_info, "Scanning thread completed");
	});

	if (api.Init(NULL, "eng", tesseract::OcrEngineMode::OEM_DEFAULT)) {
		bot.log(dpp::ll_error, "Could not initialise tesseract");
		return;
	}
	api.SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_BLOCK);
	Pix* image = pixReadMem((l_uint8*)file_content.data(), file_content.length());
	if (!image || !image->data || image->w == 0 || image->h == 0) {
		bot.log(dpp::ll_error, "Could not read image with pixRead, skipping OCR");
		if (image) {
			pixDestroy(&image);
		}
	} else {
		/* We may have already checked this value if discord gave it us as attachment metadata.
		* Just to be sure, and also in case we're processing an image given in a raw url, we check the
		* width and height again here.
		*/
		if (image->w * image->h > 33554432) {
			bot.log(dpp::ll_info, "Image dimensions of " + std::to_string(image->w) + "x" + std::to_string(image->h) + " too large to be a screenshot");
			pixDestroy(&image);
			return;
		}
		image = pixConvertRGBToGrayFast(image);
		if (image == nullptr) {
			bot.log(dpp::ll_error, "pixConvertToRGBFast made my pointer null!");
		}
		api.SetImage(image);
		pixDestroy(&image);
		const char* output = api.GetUTF8Text();
		if (!output) {
			bot.log(dpp::ll_warning, "GetUTF8Text() returned nullptr!!! Skipping OCR");
		} else {
			ocr = output;
			std::vector<std::string> lines = dpp::utility::tokenize(ocr, "\n");
			delete[] output;
			bot.log(dpp::ll_debug, "Read " + std::to_string(lines.size()) + " lines of text from image with total size " + std::to_string(ocr.length()));
			db::resultset patterns = db::query("SELECT * FROM guild_patterns WHERE guild_id = '?'", { ev.msg.guild_id.str() });
			bot.log(dpp::ll_debug, "Checking image content against " + std::to_string(patterns.size()) + " patterns...");
			for (const std::string& line : lines) {
				if (line.length() && line[0] == 0x0C) {
					/* Tesseract puts random formdeeds in the output, skip them */
					continue;
				}
				for (const db::row& pattern : patterns) {
					const std::string& p = pattern.at("pattern");
					std::string pattern_wild = "*" + p + "*";
					if (line.length() && p.length() && match(line.c_str(), pattern_wild.c_str())) {
						delete_message_and_warn(bot, ev, attach, p, false);
						std::string hash = sha256(file_content);
						db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
						return;
					}
				}
			}
		}
	}

	db::resultset settings = db::query("SELECT premium_subscription FROM guild_config WHERE guild_id = ? AND calls_this_month <= calls_limit", { ev.msg.guild_id.str() });
	if (settings.size() && settings[0].at("premium_subscription").length()) {
		try {
			EasyGifReader gif_reader = EasyGifReader::openMemory(file_content.data(), file_content.length());
			int frame_count = gif_reader.frameCount();
			if (frame_count > 1) {
				bot.log(dpp::ll_debug, "Detected animated gif with " + std::to_string(frame_count) + " frames, name: " + attach.filename + "; not scanning with IR");
				std::string hash = sha256(file_content);
				db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
				return;
			}
		} catch (const EasyGifReader::Error& error) {
			/* Not a gif, this is not a fatal error */
		}
		json& irconf = config::get("ir");
		std::vector<std::string> fields = irconf["fields"];
		std::string endpoint = irconf["endpoint"];
		db::resultset m = db::query("SELECT GROUP_CONCAT(DISTINCT model) AS selected FROM premium_filter_model");
		std::string active_models = m[0].at("selected");
		std::string url = endpoint
			+ "?" + fields[0] + "=" + dpp::utility::url_encode(active_models)
			+ "&" + fields[1] + "=" + dpp::utility::url_encode(irconf["credentials"]["username"])
			+ "&" + fields[2] + "=" + dpp::utility::url_encode(irconf["credentials"]["password"])
			+ "&" + fields[3] + "=" + dpp::utility::url_encode(attach.url);
		db::query("UPDATE guild_config SET calls_this_month = calls_this_month + 1 WHERE guild_id = ?", {ev.msg.guild_id.str() });
		bot.request(url, dpp::m_get,
			[attach, ev, &bot, url, ocr, file_content](const dpp::http_request_completion_t& premium_api) {
				if (premium_api.status == 200 && !premium_api.body.empty()) {
					json answer;
					try {
						answer = json::parse(premium_api.body);
					} catch (const std::exception& e) {
					}
					find_banned_type(answer, attach, bot, ev);
					std::string hash = sha256(file_content);
					db::query("INSERT INTO scan_cache (hash, ocr, api) VALUES('?','?','?') ON DUPLICATE KEY UPDATE ocr = '?', api = '?'", { hash, ocr, premium_api.body, ocr, premium_api.body });
				} else {
					bot.log(dpp::ll_debug, "API Error: '" + premium_api.body + "' status: " + std::to_string(premium_api.status));
					std::string hash = sha256(file_content);
					db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
				}
			}
		,"", "text/json", {{"Accept", "*/*"}}, "1.0");
	} else {
		std::string hash = sha256(file_content);
		db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
	}
}

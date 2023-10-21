#include <dpp/dpp.h>
#include <beholder/config.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include "3rdparty/EasyGifReader.h"
#include "3rdparty/httplib.h"


namespace ocr {

	tesseract::TessBaseAPI apis[max_concurrency];
	std::atomic<bool> api_busy[max_concurrency];

	void init(dpp::cluster& bot)
	{
		bot.log(dpp::ll_info, "Initialising OCR...");
		for(int z = 0; z < max_concurrency; ++z) {
			if (apis[z].Init(NULL, "eng", tesseract::OcrEngineMode::OEM_DEFAULT)) {
				bot.log(dpp::ll_error, "Could not initialise tesseract #" + std::to_string(z));
				return;
			}
		}
		bot.log(dpp::ll_info, "OCR initialised.");
	}

	tesseract::TessBaseAPI& get_tess_api(int& id)
	{
		for(int z = 0; z < max_concurrency; ++z) {
			if (api_busy[z] == false) {
				api_busy[z] = true;
				id = z;
				return apis[z];
			}
		}
		throw std::runtime_error("No free apis, this shouldnt happen!");
	}

	void free_tess_api(int id)
	{
		api_busy[id] = 0;
	}

	void image(std::string file_content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
		std::string ocr;

		int id;
		tesseract::TessBaseAPI& api = get_tess_api(id);
		dpp::utility::set_thread_name("img-scan/" + std::to_string(id));

		on_thread_exit([&bot, id]() {
			concurrent_images--;
			free_tess_api(id);
			bot.log(dpp::ll_info, "Scanning thread " + std::to_string(id) + " completed");
		});

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
			api.SetImage(image);
			pixDestroy(&image);
			const char* output = api.GetUTF8Text();
			api.Clear();
 			if (!output) {
				bot.log(dpp::ll_warning, "GetUTF8Text() returned nullptr!!! Skipping OCR");
			} else {
				ocr = output;
				delete[] output;
				std::vector<std::string> lines = dpp::utility::tokenize(ocr, "\n");
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
							delete_message_and_warn(file_content, bot, ev, attach, p, false);
							std::string hash = sha256(file_content);
							db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
							bot.log(dpp::ll_error, "done 1");
							return;
						}
					}
				}
			}
		}

		db::resultset settings = db::query("SELECT premium_subscription FROM guild_config WHERE guild_id = ? AND calls_this_month <= calls_limit", { ev.msg.guild_id.str() });
		if (settings.size() && settings[0].at("premium_subscription").length()) {
			try {
				bot.log(dpp::ll_error, "ezgifreader");
				EasyGifReader gif_reader = EasyGifReader::openMemory(file_content.data(), file_content.length());
				bot.log(dpp::ll_error, "ezg 1");
				int frame_count = gif_reader.frameCount();
				bot.log(dpp::ll_error, "ezg 2");
				if (frame_count > 1) {
					bot.log(dpp::ll_debug, "Detected animated gif with " + std::to_string(frame_count) + " frames, name: " + attach.filename + "; not scanning with IR");
					std::string hash = sha256(file_content);
					db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
					return;
				}
			} catch (const EasyGifReader::Error& error) {
				/* Not a gif, this is not a fatal error */
				bot.log(dpp::ll_error, "not gif");
			}
			json& irconf = config::get("ir");
			std::vector<std::string> fields = irconf["fields"];
			std::string endpoint = irconf["host"];
			db::resultset m = db::query("SELECT GROUP_CONCAT(DISTINCT model) AS selected FROM premium_filter_model");
			std::string active_models = m[0].at("selected");
			std::string url = irconf["path"].get<std::string>()
				+ "?" + fields[0] + "=" + dpp::utility::url_encode(active_models)
				+ "&" + fields[1] + "=" + dpp::utility::url_encode(irconf["credentials"]["username"])
				+ "&" + fields[2] + "=" + dpp::utility::url_encode(irconf["credentials"]["password"])
				+ "&" + fields[3] + "=" + dpp::utility::url_encode(attach.url);
			db::query("UPDATE guild_config SET calls_this_month = calls_this_month + 1 WHERE guild_id = ?", {ev.msg.guild_id.str() });

			/* Build httplib client */
			bot.log(dpp::ll_debug, "Host: " + endpoint + " url: " + url);
			httplib::Client cli(endpoint.c_str());
			cli.enable_server_certificate_verification(false);
			std::string rv;
			int code = 0;
			auto res = cli.Get(url.c_str());
			if (res) {
				url = endpoint + url;
				if (res->status < 400) {
					json answer;
					try {
						answer = json::parse(res->body);
					} catch (const std::exception& e) {
					}
					find_banned_type(answer, attach, bot, ev, file_content);
					std::string hash = sha256(file_content);
					db::query("INSERT INTO scan_cache (hash, ocr, api) VALUES('?','?','?') ON DUPLICATE KEY UPDATE ocr = '?', api = '?'", { hash, ocr, res->body, ocr, res->body });
				} else {
					bot.log(dpp::ll_debug, "API Error: '" + res->body + "' status: " + std::to_string(res->status));
					std::string hash = sha256(file_content);
					db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
				}
			} else {
				bot.log(dpp::ll_debug, "API Error(2) " + res->body + " " + std::to_string(res->status));
				std::string hash = sha256(file_content);
				db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
			}
		} else {
			std::string hash = sha256(file_content);
			db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
		}
	}

}
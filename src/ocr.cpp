#include <dpp/dpp.h>
#include <beholder/config.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include "3rdparty/httplib.h"
#include <fmt/format.h>
#include <beholder/proc/cpipe.h>
#include <beholder/proc/spawn.h>
#include <beholder/tessd.h>

namespace ocr {

	void image(std::string file_content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
		std::string ocr;

		dpp::utility::set_thread_name("img-scan/" + std::to_string(concurrent_images));

		on_thread_exit([&bot]() {
			bot.log(dpp::ll_info, "Scanning thread completed");
			concurrent_images--;
		});

		std::string hash = sha256(file_content);

		/* This loop is not redundant - it ensures the spawn class is scoped */
		try {
			const char* const argv[] = {"./tessd", nullptr};
			spawn tessd(argv);
			bot.log(dpp::ll_info, fmt::format("spawned tessd; pid={}", tessd.get_pid()));
			tessd.stdin.write(file_content.data(), file_content.length());
			tessd.send_eof();
			std::string l;
			while (std::getline(tessd.stdout, l)) {
				ocr.append(l).append("\n");
			}
			int ret = tessd.wait();
			if (ret > static_cast<int>(tessd::exit_code::no_error) - 1 && ret < static_cast<int>(tessd::exit_code::max)) {
				bot.log(ret ? dpp::ll_error : dpp::ll_info, fmt::format("tessd status {}: {}", ret, tessd::tessd_error[ret]));
			}
		} catch (const std::runtime_error& e) {
			bot.log(dpp::ll_error, e.what());
		}

		if (!ocr.empty()) {
			std::vector<std::string> lines = dpp::utility::tokenize(ocr, "\n");
			bot.log(dpp::ll_debug, "Read " + std::to_string(lines.size()) + " lines of text from image with total size " + std::to_string(ocr.length()));
			db::resultset patterns = db::query("SELECT * FROM guild_patterns WHERE guild_id = '?'", { ev.msg.guild_id.str() });
			bot.log(dpp::ll_debug, "Checking image content against " + std::to_string(patterns.size()) + " patterns...");
			for (const std::string& line : lines) {
				for (const db::row& pattern : patterns) {
					const std::string& p = pattern.at("pattern");
					std::string pattern_wild = "*" + p + "*";
					if (line.length() && p.length() && match(line.c_str(), pattern_wild.c_str())) {
						delete_message_and_warn(file_content, bot, ev, attach, p, false);
						db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
						return;
					}
				}
			}
		} else {
			bot.log(dpp::ll_debug, "No OCR content in image");
		}

		db::resultset settings = db::query("SELECT premium_subscription FROM guild_config WHERE guild_id = ? AND calls_this_month <= calls_limit", { ev.msg.guild_id.str() });
		/* Only images of >= 50 pixels in both dimensions and smaller than 12mb are supported by the API. Anything else we dont scan. 
		 * In the event we dont have the dimensions, scan it anyway.
		 */
		if (attach.size < 1024 * 1024 * 12 &&
			(attach.width == 0 || attach.width >= 50) && (attach.height == 0 || attach.height >= 50) && settings.size() && settings[0].at("premium_subscription").length()) {
			/* Animated gifs require a control structure only available in GIF89a, GIF87a is fine and anything that is
			 * neither is not a GIF file.
			 * By the way, it's pronounced GIF, as in GOLF, not JIF, as in JUMP! 🤣
			 */
			uint8_t* filebits = reinterpret_cast<uint8_t*>(file_content.data());
			if (file_content.length() >= 6 && filebits[0] == 'G' && filebits[1] == 'I' && filebits[2] == 'F' && filebits[3] == '8' && filebits[4] == '9' && filebits[5] == 'a') {
				/* If control structure is found, sequence 21 F9 04, we dont pass the gif to the API as it is likely animated
				 * This is a much faster, more lightweight check than using a GIF library.
				 */
				for (size_t x = 0; x < file_content.length() - 3; ++x) {
					if (filebits[x] == 0x21 && filebits[x + 1] == 0xF9 && filebits[x + 2] == 0x04) {
						bot.log(dpp::ll_debug, "Detected animated gif, name: " + attach.filename + "; not scanning with IR");
						db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
						return;
					}
				}
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
			httplib::Client cli(endpoint.c_str());
			cli.enable_server_certificate_verification(false);
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
					db::query("INSERT INTO scan_cache (hash, ocr, api) VALUES('?','?','?') ON DUPLICATE KEY UPDATE ocr = '?', api = '?'", { hash, ocr, res->body, ocr, res->body });
				} else {
					bot.log(dpp::ll_debug, "API Error: '" + res->body + "' status: " + std::to_string(res->status));
					db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
				}
			} else {
				bot.log(dpp::ll_debug, "API Error; failed to connect");
				db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
			}
		} else {
			db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
		}
	}

}

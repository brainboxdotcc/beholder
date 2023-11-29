/************************************************************************************
 * 
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#include <dpp/dpp.h>
#include <beholder/config.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <fmt/format.h>
#include <beholder/proc/cpipe.h>
#include <beholder/proc/spawn.h>
#include <beholder/tessd.h>
#include <beholder/ocr.h>
#include <beholder/image.h>
#include <exception>

namespace ocr {

	bool scan(bool &flattened, const std::string& hash, std::string& file_content, const dpp::attachment& attach, dpp::cluster& bot, const dpp::message_create_t ev, int pass, bool delete_message) {
		db::resultset pattern_count = db::query("SELECT COUNT(guild_id) AS total FROM guild_patterns WHERE guild_id = ? AND pattern NOT LIKE '!%'", { ev.msg.guild_id });
		std::string ocr;
		if (pattern_count.size() > 0 && atoi(pattern_count[0].at("total").c_str()) > 0) {
			try {
				db::resultset rs = db::query("SELECT * FROM scan_cache WHERE hash = ?", { hash });
				if (!rs.empty()) {
					ocr = rs[0].at("ocr");
					bot.log(dpp::ll_info, fmt::format("image {} is in OCR cache", hash));
					INCREMENT_STATISTIC("cache_hit", ev.msg.guild_id);
				} else {
					if (!flattened) {
						file_content = image::flatten_gif(bot, attach, file_content);
						flattened = true;
					}
					INCREMENT_STATISTIC("cache_miss", ev.msg.guild_id);
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
					if (!ocr.empty()) {
						db::query("INSERT INTO scan_cache (hash, ocr) VALUES(?,?) ON DUPLICATE KEY UPDATE ocr = ?", { hash, ocr, ocr });
					}
				}
			} catch (const std::runtime_error& e) {
				bot.log(dpp::ll_error, e.what());
			}

			if (!ocr.empty()) {
				std::vector<std::string> lines = dpp::utility::tokenize(ocr, "\n");
				bot.log(dpp::ll_debug, "Read " + std::to_string(lines.size()) + " lines of text from image with total size " + std::to_string(ocr.length()));
				db::resultset patterns = db::query("SELECT * FROM guild_patterns WHERE guild_id = ? AND pattern NOT LIKE '!%'", { ev.msg.guild_id });
				bot.log(dpp::ll_debug, "Checking image content against " + std::to_string(patterns.size()) + " patterns...");
				for (const std::string& line : lines) {
					for (const db::row& pattern : patterns) {
						const std::string& p = replace_string(pattern.at("pattern"), "\r", "");
						std::string pattern_wild = "*" + p + "*";
						if (line.length() && p.length() && match(line.c_str(), pattern_wild.c_str())) {
							if (delete_message) {
								delete_message_and_warn(hash, file_content, bot, ev, attach, p, false);
							} else {
								throw std::runtime_error(p.c_str());
							}
							INCREMENT_STATISTIC("images_ocr", ev.msg.guild_id);
							return true;
						}
					}
				}
			} else {
				bot.log(dpp::ll_debug, "No OCR content in image");
			}
		} else {
			bot.log(dpp::ll_debug, "No OCR patterns configured in guild " + ev.msg.guild_id.str());
		}
		return false;
	}
}

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
#include <typeinfo>
#include <dpp/dpp.h>
#include <beholder/config.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include "3rdparty/httplib.h"
#include <fmt/format.h>
#include <beholder/proc/cpipe.h>
#include <beholder/proc/spawn.h>
#include <beholder/tessd.h>
#include <beholder/sentry.h>

namespace ocr {

	/**
	 * @brief Given an image file, check if it is a gif, and if it is animated.
	 * If it is, flatten it by extracting just the first frame using imagemagick.
	 * 
	 * @param bot Reference to D++ cluster
	 * @param attach message attachment
	 * @param file_content file content
	 * @return std::string new file content
	 */
	std::string flatten_gif(dpp::cluster& bot, const dpp::attachment attach, std::string file_content) {
		if (!attach.filename.ends_with(".gif") && !attach.filename.ends_with(".GIF")) {
			return file_content;
		}
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
					bot.log(dpp::ll_debug, "Detected animated gif, name: " + attach.filename + "; flattening with convert");
					const char* const argv[] = {"/usr/bin/convert", "-[0]", "/dev/stdout", nullptr};
					spawn convert(argv);
					bot.log(dpp::ll_info, fmt::format("spawned convert; pid={}", convert.get_pid()));
					convert.stdin.write(file_content.data(), file_content.length());
					convert.send_eof();
					std::ostringstream stream;
					stream << convert.stdout.rdbuf();
					file_content = stream.str();
					int ret = convert.wait();
					bot.log(ret ? dpp::ll_error : dpp::ll_info, fmt::format("convert status {}", ret));
					return file_content;
				}
			}
		}
		return file_content;
	}

	void image(std::string file_content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
		std::string ocr;
		dpp::utility::set_thread_name("img-scan/" + std::to_string(concurrent_images));

		on_thread_exit([&bot]() {
			bot.log(dpp::ll_info, "Scanning thread completed");
			concurrent_images--;
		});

		bool flattened = false;
		std::string hash = sha256(file_content);

		db::resultset pattern_count = db::query("SELECT COUNT(guild_id) AS total FROM guild_patterns WHERE guild_id = ?", { ev.msg.guild_id.str() });
		if (pattern_count.size() > 0 && atoi(pattern_count[0].at("total").c_str()) > 0) {
			try {
				db::resultset rs = db::query("SELECT * FROM scan_cache WHERE hash = ?", { hash });
				if (!rs.empty()) {
					ocr = rs[0].at("ocr");
					bot.log(dpp::ll_info, fmt::format("image {} is in OCR cache", hash));
				} else {
					if (!flattened) {
						file_content = flatten_gif(bot, attach, file_content);
						flattened = true;
					}
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
				db::resultset patterns = db::query("SELECT * FROM guild_patterns WHERE guild_id = ?", { ev.msg.guild_id.str() });
				bot.log(dpp::ll_debug, "Checking image content against " + std::to_string(patterns.size()) + " patterns...");
				for (const std::string& line : lines) {
					for (const db::row& pattern : patterns) {
						const std::string& p = pattern.at("pattern");
						std::string pattern_wild = "*" + p + "*";
						if (line.length() && p.length() && match(line.c_str(), pattern_wild.c_str())) {
							delete_message_and_warn(file_content, bot, ev, attach, p, false);
							return;
						}
					}
				}
			} else {
				bot.log(dpp::ll_debug, "No OCR content in image");
			}
		} else {
			bot.log(dpp::ll_debug, "No OCR patterns configured in guild " + ev.msg.guild_id.str());
		}

		db::resultset settings = db::query("SELECT premium_subscription FROM guild_config WHERE guild_id = ? AND calls_this_month <= calls_limit", { ev.msg.guild_id.str() });
		/* Only images of >= 50 pixels in both dimensions and smaller than 12mb are supported by the API. Anything else we dont scan. 
		 * In the event we dont have the dimensions, scan it anyway.
		 */
		if (attach.size < 1024 * 1024 * 12 &&
			(attach.width == 0 || attach.width >= 50) && (attach.height == 0 || attach.height >= 50) && settings.size() && settings[0].at("premium_subscription").length()) {
			json& irconf = config::get("ir");
			std::vector<std::string> fields = irconf["fields"];
			std::string endpoint = irconf["host"];
			db::resultset m = db::query("SELECT GROUP_CONCAT(DISTINCT model) AS selected FROM premium_filter_model WHERE category IN (SELECT pattern FROM premium_filters WHERE guild_id = ?)", {ev.msg.guild_id});
			std::string active_models = m[0].at("selected");
			std::string original_active = active_models;
			std::vector<std::string> active = dpp::utility::tokenize(active_models, ",");
			std::vector<std::string> models = active;
			json merge;
			if (std::find(active.begin(), active.end(), "wad") != active.end()) {
				db::resultset rs = db::query("SELECT * FROM api_cache_wad WHERE hash = ?", { hash });
				if (rs.size()) {
					active.erase(std::remove(active.begin(), active.end(), "wad"), active.end());
					json p = json::parse(rs[0].at("api"));
					merge["weapon"] = p[0].get<double>();
					merge["alcohol"] = p[1].get<double>();
					merge["drugs"] = p[2].get<double>();
					bot.log(dpp::ll_debug, "Got cached WAD: " + merge.dump());
				}
			}
			if (std::find(active.begin(), active.end(), "gore") != active.end()) {
				db::resultset rs = db::query("SELECT * FROM api_cache_gore WHERE hash = ?", { hash });
				if (rs.size()) {
					active.erase(std::remove(active.begin(), active.end(), "gore"), active.end());
					merge["gore"] = json::parse(rs[0].at("api"));
					bot.log(dpp::ll_debug, "Got cached gore: " + json::parse(rs[0].at("api")).dump());
				}
			}
			if (std::find(active.begin(), active.end(), "nudity-2.0") != active.end()) {
				db::resultset rs = db::query("SELECT * FROM api_cache_nudity WHERE hash = ?", { hash });
				if (rs.size()) {
					active.erase(std::remove(active.begin(), active.end(), "nudity-2.0"), active.end());
					merge["nudity"] = json::parse(rs[0].at("api"));
					bot.log(dpp::ll_debug, "Got cached nudity-2.0: " + json::parse(rs[0].at("api")).dump());
				}
			}
			if (std::find(active.begin(), active.end(), "offensive") != active.end()) {
				db::resultset rs = db::query("SELECT * FROM api_cache_offensive WHERE hash = ?", { hash });
				if (rs.size()) {
					active.erase(std::remove(active.begin(), active.end(), "offensive"), active.end());
					merge["offensive"] = json::parse(rs[0].at("api"));
					bot.log(dpp::ll_debug, "Got cached offensive: " + json::parse(rs[0].at("api")).dump());
				}
			}
			active_models.clear();
			for (const std::string a : active) {
				active_models.append(a).append(",");
			}
			if (active_models.length()) {
				active_models = active_models.substr(0, active_models.length() - 1);
			}

			if (!original_active.empty()) {

				json answer;

				if (active.empty()) {
					bot.log(dpp::ll_info, fmt::format("image {} has all models in API cache ({})", hash, original_active));
					answer = merge;
					find_banned_type(answer, attach, bot, ev, file_content);
		 		} else {
					if (!flattened) {
						file_content = flatten_gif(bot, attach, file_content);
						flattened = true;
					}
					/* Make API request, upload the image, don't get the API to download it.
					* This is more expensive for us in terms of bandwidth, but we are going
					* to be able to check more images more of the time this way. We already
					* have the image data in memory and can upload it straight to the API.
					* Asking the API endpoint to download it via URL risks them being rate
					* limited or blocked as they will be making many hundreds of requests
					* per minute and we will not.
					*/
					httplib::Client cli(endpoint.c_str());
					cli.enable_server_certificate_verification(false);
					httplib::MultipartFormDataItems items = {
						{ fields[4], file_content, attach.filename, "application/octet-stream" },
						{ fields[1], irconf["credentials"]["username"], "", "" },
						{ fields[2], irconf["credentials"]["password"], "", "" },
						{ fields[0], active_models, "", "" },
					};
					auto res = cli.Post(irconf["path"].get<std::string>().c_str(), items);

					if (res) {
						try {
							answer.merge_patch(json::parse(res->body));
						}
						catch (const std::exception &e) {
						}
						if (res->status < 400) {
							bot.log(dpp::ll_info, "API response ID: " + answer["request"]["id"].get<std::string>() + " with models: " + original_active);
							find_banned_type(answer, attach, bot, ev, file_content);
							
							if (std::find(models.begin(), models.end(), "nudity-2.0") != models.end()) {
								std::string v{answer.at("nudity").dump()};
								db::query("INSERT INTO api_cache_nudity (hash, api) VALUES(?,?) ON DUPLICATE KEY UPDATE api = ?", { hash, v, v });
								bot.log(dpp::ll_debug, "Cached nudity-2.0: " + v);
							}
							if (std::find(models.begin(), models.end(), "gore") != models.end()) {
								std::string v{answer.at("gore").dump()};
								db::query("INSERT INTO api_cache_gore (hash, api) VALUES(?,?) ON DUPLICATE KEY UPDATE api = ?", { hash, v, v });
								bot.log(dpp::ll_debug, "Cached gore: " + v);
							}
							if (std::find(models.begin(), models.end(), "offensive") != models.end()) {
								std::string v{answer.at("offensive").dump()};
								db::query("INSERT INTO api_cache_offensive (hash, api) VALUES(?,?) ON DUPLICATE KEY UPDATE api = ?", { hash, v, v });
								bot.log(dpp::ll_debug, "Cached offensive: " + v);
							}
							if (std::find(models.begin(), models.end(), "wad") != models.end()) {
								std::string v = "[" + 
									std::to_string(answer.at("weapon").get<double>()) + "," +
									std::to_string(answer.at("alcohol").get<double>()) + "," +
									std::to_string(answer.at("drugs").get<double>()) + "]";
								db::query("INSERT INTO api_cache_wad (hash, api) VALUES(?,?) ON DUPLICATE KEY UPDATE api = ?", { hash, v, v });
								bot.log(dpp::ll_debug, "Cached wad: " + v);
							}
							db::query("UPDATE guild_config SET calls_this_month = calls_this_month + 1 WHERE guild_id = ?", {ev.msg.guild_id.str() });
						} else {
							if (answer.contains("error") && answer.contains("request")) {
								bot.log(dpp::ll_warning, "API Error: '" + std::to_string(answer["error"]["code"].get<int>()) + "; " + answer["error"]["message"].get<std::string>()  + "' status: " + std::to_string(res->status) + " id: " + answer["request"]["id"].get<std::string>());
							}
						}
					} else {
						auto err = res.error();
						bot.log(dpp::ll_warning, fmt::format("API Error: {}", httplib::to_string(err)));
					}
				}
			}
		}
	}

}

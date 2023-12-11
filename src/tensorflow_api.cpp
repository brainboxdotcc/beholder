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
#include <beholder/image.h>
#include "3rdparty/httplib.h"

namespace tensorflow_api {

	bool scan(bool &flattened, const std::string& hash, std::string& file_content, const dpp::attachment& attach, dpp::cluster& bot, const dpp::message_create_t ev, int pass, bool delete_message) {
		db::resultset settings = db::query("SELECT basic_nsfw_suggestive, basic_nsfw_porn, basic_nsfw_drawing, basic_nsfw_hentai FROM guild_config WHERE guild_id = ?", { ev.msg.guild_id });
		if (settings.size() == 0) {
			/* Unconfigured defaults */
			bot.log(dpp::ll_debug, "Guild " + ev.msg.guild_id.str() + " using unconfigured basic scan defaults");
			settings = db::query("SELECT 1 as basic_nsfw_suggestive, 1 as basic_nsfw_porn, 0 as basic_nsfw_drawing, 1 as basic_nsfw_hentai");
		}
		json answer;
		try {
			db::resultset rs = db::query("SELECT * FROM basic_cache WHERE hash = ?", { hash });
			if (!rs.empty()) {
				answer = json::parse(rs[0].at("basic"));
				bot.log(dpp::ll_info, fmt::format("image {} is in basic cache", hash));
				INCREMENT_STATISTIC("cache_hit", ev.msg.guild_id);
			} else {
				if (!flattened) {
					file_content = image::flatten_gif(bot, attach, file_content);
					flattened = true;
				}
				httplib::Client cli("http://localhost:6969");
				auto res = cli.Post("/", file_content, "application/octet-stream");
				if (res) {
					if (res->status < 400) {
						answer = json::parse(res->body);
						if (!answer.contains("error")) {
							db::query("INSERT INTO basic_cache (hash, basic) VALUES(?,?) ON DUPLICATE KEY UPDATE hash = ?", { hash, res->body, res->body });
						} else {
							bot.log(dpp::ll_warning, fmt::format("Basic NSFW API Error: {}", answer.at("error")));
							return false;
						}
					}
				} else {
					auto err = res.error();
					bot.log(dpp::ll_warning, fmt::format("Basic NSFW API Error: {}", httplib::to_string(err)));
					return false;
				}
				INCREMENT_STATISTIC("cache_miss", ev.msg.guild_id);
			}
		} catch (const std::runtime_error& e) {
			bot.log(dpp::ll_error, e.what());
			return false;
		}

		if (!answer.empty()) {
			std::string block_reason;
			if (settings[0].at("basic_nsfw_suggestive") == "1") {
				double score = answer.at("sexy").get<double>();
				if (score > 0.8) {
					block_reason = fmt::format("Basic NSFW: Suggestive ({0:.02f}{1})", score * 100, '%');
				}
			}
			if (settings[0].at("basic_nsfw_porn") == "1") {
				double score = answer.at("porn").get<double>();
				if (score > 0.8) {
					block_reason = fmt::format("Basic NSFW: Pornography ({0:.02f}{1})", score * 100, '%');
				}
			}
			if (settings[0].at("basic_nsfw_drawing") == "1") {
				double score = answer.at("drawing").get<double>();
				if (score > 0.95) {
					block_reason = fmt::format("Basic NSFW: Drawing ({0:.02f}{1})", score * 100, '%');
				}
			}
			if (settings[0].at("basic_nsfw_hentai") == "1") {
				double score = answer.at("hentai").get<double>();
				if (score > 0.8) {
					block_reason = fmt::format("Basic NSFW: Hentai ({0:.02f}{1})", score * 100, '%');
				}
			}

			if (!block_reason.empty()) {
				bot.log(dpp::ll_debug, "Detected Basic NSFW: " + block_reason);
				if (delete_message) {
					delete_message_and_warn(hash, file_content, bot, ev, attach, block_reason, false);
				} else {
					throw std::runtime_error(block_reason);
				}
				INCREMENT_STATISTIC("images_nsfw", ev.msg.guild_id);
				return true;
			}
		} else {
			bot.log(dpp::ll_debug, "No basic NSFW content in image");
		}
		return false;
	}
}

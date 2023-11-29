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

namespace label {

	bool scan(bool &flattened, const std::string& hash, std::string& file_content, const dpp::attachment& attach, dpp::cluster& bot, const dpp::message_create_t ev, int pass, bool delete_message) {
		db::resultset settings = db::query("SELECT guild_id FROM guild_config WHERE guild_id = ? AND objects_this_month <= object_limit", { ev.msg.guild_id });
		if (settings.empty()) {
			/* Not configured, or out of quota */
			return false;
		}
		db::resultset label_patterns = db::query("SELECT pattern FROM guild_patterns WHERE guild_id = ? AND pattern LIKE '!%'", { ev.msg.guild_id });
		if (label_patterns.size() > 0) {
			json answer;
			try {
				db::resultset rs = db::query("SELECT * FROM label_cache WHERE hash = ?", { hash });
				if (!rs.empty()) {
					answer = json::parse(rs[0].at("label"));
					bot.log(dpp::ll_info, fmt::format("image {} is in label cache", hash));
					INCREMENT_STATISTIC("cache_hit", ev.msg.guild_id);
				} else {
					json conf = config::get("ir");
					std::string runner = conf.at("label_runner");
					std::string key = conf.at("label_key");
					if (!flattened) {
						file_content = image::flatten_gif(bot, attach, file_content);
						flattened = true;
					}
					httplib::Client cli("https://api.cloudflare.com");
					cli.enable_server_certificate_verification(false);
					cli.set_default_headers({
						{"Authorization", "Bearer " + key}
					});
					auto res = cli.Post(runner.c_str(), file_content, "application/octet-stream");
					if (res) {
						if (res->status < 400) {
							answer = json::parse(res->body);
							db::query("INSERT INTO label_cache (hash, label) VALUES(?,?) ON DUPLICATE KEY UPDATE label = ?", { hash, res->body, res->body });
							db::query("UPDATE guild_config SET objects_this_month = objects_this_month + 1 WHERE guild_id = ?", {ev.msg.guild_id });
						}
					} else {
						auto err = res.error();
						bot.log(dpp::ll_warning, fmt::format("Label API Error: {}", httplib::to_string(err)));
					}
					INCREMENT_STATISTIC("cache_miss", ev.msg.guild_id);
				}
			} catch (const std::runtime_error& e) {
				bot.log(dpp::ll_error, e.what());
			}

			if (!answer.empty()) {
			
				bot.log(dpp::ll_debug, "Checking image labels against " + std::to_string(label_patterns.size()) + " patterns...");
				std::string query{"SELECT hash, group_concat(label) as labels FROM vw_label_confidence WHERE hash = ? GROUP BY hash HAVING (1=1 "};
				std::string human{"Contains"};
				db::paramlist params;
				params.emplace_back(hash);
				size_t pos = 0;
				for (const db::row& pattern : label_patterns) {
					std::string pat = pattern.at("pattern");
					pat = replace_string(pat.substr(1, pat.length() - 1), "\r", "");
					if (pat[0] == '!') {
						query += "AND (labels NOT LIKE ?)";
						std::string thing = pat.substr(1, pat.length() - 1);
						params.emplace_back('%' + thing + '%');
						human += fmt::format(" no '{0}'", thing);
					} else {
						query += "AND (labels LIKE ?)";
						params.emplace_back('%' + pat + '%');
						human += fmt::format(" any '{0}'", pat);
					}
					if (pos++ != label_patterns.size() - 1) {
						human += ", and ";
					}
				}
				query += ")";
				auto check = db::query(query, params);
				if (!check.empty()) {
					if (delete_message) {
						delete_message_and_warn(hash, file_content, bot, ev, attach, human, false);
					} else {
						throw std::runtime_error(human);
					}
					INCREMENT_STATISTIC("images_label", ev.msg.guild_id);
					return true;

				}
			} else {
				bot.log(dpp::ll_debug, "No label content in image");
			}
		} else {
			bot.log(dpp::ll_debug, "No label patterns configured in guild " + ev.msg.guild_id.str());
		}
		return false;
	}
}

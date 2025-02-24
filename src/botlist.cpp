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
#include <beholder/botlist.h>
#include <beholder/database.h>
#include <beholder/config.h>

/**
 * @brief Internal list of botlist site configs
 */
static registered_botlist_list registered_botlists;

registered_botlist_list& get_botlist_map() {
	return registered_botlists;
}

void post_botlists(dpp::cluster &bot) {
	for (const auto & botlist : registered_botlists) {
		auto ptr = botlist.second;
		(*ptr)(bot);
	}
}

void botlist::run(dpp::cluster& bot, const std::string_view key, const std::string_view url, const std::string_view count_field, const std::string_view shards_field) {
	const json list_config = config::get("botlists");
	auto rs = db::query("SELECT COUNT(id) AS count FROM guild_cache");
	if (list_config.contains(key.data())) {
		const json& topgg_config = list_config.at(key.data());
		std::string token = topgg_config.at("token");
		json j;
		if (!shards_field.empty()) {
			j[shards_field.data()] = bot.numshards;
		}
		if (!count_field.empty() && !rs.empty()) {
			j[count_field.data()] = atoi(rs[0].at("count").c_str());
		}
		std::string post_url = replace_string(url.data(), "{}", bot.me.id.str());
		bot.request(post_url, dpp::m_post, [&bot, key](const auto & cc){
			if (cc.status >= 400) {
				bot.log(dpp::ll_warning, std::string(key) + " returned: " + cc.body);
			}
		}, j.dump(), "application/json", {{"Authorization", token}});
	}
}

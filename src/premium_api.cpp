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
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <dpp/json.h>

bool find_banned_type(const json& response, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev, const std::string& content)
{
	db::resultset premium_filters = db::query("SELECT * FROM premium_filters WHERE guild_id = ?", { ev.msg.guild_id });
	bot.log(dpp::ll_debug, std::to_string(premium_filters.size()) + " premium filters to check");
	for (const db::row& row : premium_filters) {
		std::string filter_type = row.at("pattern");
		bot.log(dpp::ll_debug, "check filter " + filter_type);
		double trigger_value = 0.8, found_value = 0;
		if (row.at("score").length()) {
			trigger_value = atof(row.at("score").c_str());
		}
		json::json_pointer ptr("/" + replace_string(filter_type, ".", "/"));
		json value = response[ptr];
		if (!value.empty()) {
			if (value.is_number_float() || value.is_number_integer()) {
				found_value = value.get<double>();
				bot.log(dpp::ll_debug, "found value " + std::to_string(found_value));
			}
		} else {
			bot.log(dpp::ll_debug, "not found value");
			found_value = 0.0;
		}
		if (found_value >= trigger_value && found_value != 0.0) {
			bot.log(dpp::ll_info, "Matched premium filter " + filter_type + " value " + std::to_string(found_value));
			auto filter_name = db::query("SELECT description FROM premium_filter_model WHERE category = ?", { filter_type });
			std::string human_readable = filter_name[0].at("description");
			delete_message_and_warn(content, bot, ev, attach, human_readable, true, found_value, trigger_value);
			return true;
		}
	}

	return false;
}
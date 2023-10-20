#include <dpp/dpp.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <dpp/json.h>

bool find_banned_type(const json& response, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev)
{
	std::string status = response.at("status");
	if (status != "success") {
		bot.log(dpp::ll_info, "API status was: " + status);
		return false;
	}

	db::resultset premium_filters = db::query("SELECT * FROM premium_filters WHERE guild_id = ?", { ev.msg.guild_id.str() });
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
			auto filter_name = db::query("SELECT description FROM premium_filter_model WHERE category = '?'", { filter_type });
			std::string human_readable = filter_name[0].at("description");
			delete_message_and_warn(bot, ev, attach, human_readable, true);
			return true;
		}
	}

	return false;
}
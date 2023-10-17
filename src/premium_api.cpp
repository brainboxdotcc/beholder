#include <dpp/dpp.h>
#include <yeet/yeet.h>
#include <yeet/database.h>
#include <dpp/json.h>

extern std::atomic<int> concurrent_images;
extern json configdocument;

bool find_banned_type(const json& response, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev)
{
	std::string status = response.at("status");
	if (status != "success") {
		bot.log(dpp::ll_info, "API status was: " + status);
		return false;
	}

	db::resultset premium_filters = db::query("SELECT * FROM premium_filters WHERE guild_id = '?'");
	for (const db::row& row : premium_filters) {
		std::string filter_str = row.at("pattern");
		std::vector<std::string> filter_data = dpp::utility::tokenize(filter_str, "=");
		double trigger_value = 0.8, found_value = 0;
		if (filter_data.size() > 1) {
			trigger_value = atof(filter_data[1].c_str());
		}
		std::string filter_type = filter_data[0];
		json::json_pointer p(filter_type);
		if (!p.empty()) {
			json value = p;
			if (value.is_number_float() || value.is_number_integer()) {
				found_value = value.get<double>();
			}
		} else {
			found_value = 0.0;
		}
	}

	return false;
}
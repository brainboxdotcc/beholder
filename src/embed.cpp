#include <dpp/dpp.h>

void embed(dpp::cluster &bot, uint32_t colour, dpp::snowflake channel_id, const std::string &message, const std::string& url = "", dpp::message ref = {}, const std::string& title = "") {
	dpp::message m = dpp::message(channel_id, url);
	m.add_embed(dpp::embed().set_description(message).set_title(title).set_color(colour));
	if (ref.id) {
		m.set_reference(ref.id, ref.guild_id, channel_id);
	}
	bot.message_create(m, [&bot](const dpp::confirmation_callback_t &callback) {
		if (callback.is_error()) {
			bot.log(dpp::ll_error, "Failed to send message: " + callback.http_info.body);
		}
	});
}

void good_embed(dpp::cluster &bot, dpp::snowflake channel_id, const std::string &message, const std::string& url) {
	embed(bot, 0x7aff7a, channel_id, message, url, {}, "Yeet!");
}

void bad_embed(const std::string& title, dpp::cluster &bot, dpp::snowflake channel_id, const std::string &message, dpp::message ref) {
	embed(bot, 0xff7a7a, channel_id, message, "", ref, title);
}

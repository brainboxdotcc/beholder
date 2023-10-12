#include <dpp/dpp.h>

void embed(dpp::cluster &bot, uint32_t colour, dpp::snowflake channel_id, const std::string &message, const std::string& url = "", dpp::message ref = {}) {
	dpp::message m = dpp::message(channel_id, url);
	m.add_embed(dpp::embed().set_description(message).set_title("Yeet!").set_color(colour));
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
	embed(bot, 0x7aff7a, channel_id, message, url);
}

void bad_embed(dpp::cluster &bot, dpp::snowflake channel_id, const std::string &message, dpp::message ref) {
	embed(bot, 0xff7a7a, channel_id, message, "", ref);
}

void embed(dpp::commandhandler &ch, dpp::command_source src, uint32_t colour, const std::string &message) {
	ch.reply(dpp::message(src.channel_id, "").add_embed(dpp::embed().set_description(message).set_title("Yeet!").set_color(colour)), src);
}

void good_embed(dpp::commandhandler &ch, dpp::command_source src,  const std::string &message) {
	embed(ch, src, 0x7aff7a, message);
}

void bad_embed(dpp::commandhandler &ch, dpp::command_source src, const std::string &message) {
	embed(ch, src, 0xff7a7a, message);
}


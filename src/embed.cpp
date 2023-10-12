#include <dpp/dpp.h>

void embed(dpp::cluster &bot, uint32_t colour, dpp::snowflake channel_id, const std::string &message) {
	bot.message_create(dpp::message(channel_id, "").add_embed(dpp::embed().set_description(message).set_title("Pickle Rick!").set_color(colour))
	, [&](const dpp::confirmation_callback_t &callback) {
		if (callback.is_error()) {
			bot.log(dpp::ll_error, "Failed to send message: " + callback.http_info.body);
		}
	});
}

void good_embed(dpp::cluster &bot, dpp::snowflake channel_id, const std::string &message) {
	embed(bot, 0x7aff7a, channel_id, message);
}

void bad_embed(dpp::cluster &bot, dpp::snowflake channel_id, const std::string &message) {
	embed(bot, 0xff7a7a, channel_id, message);
}

void embed(dpp::commandhandler &ch, dpp::command_source src, uint32_t colour, const std::string &message) {
	ch.reply(dpp::message(src.channel_id, "").add_embed(dpp::embed().set_description(message).set_title("Pickle Rick!").set_color(colour)), src);
}

void good_embed(dpp::commandhandler &ch, dpp::command_source src,  const std::string &message) {
	embed(ch, src, 0x7aff7a, message);
}

void bad_embed(dpp::commandhandler &ch, dpp::command_source src, const std::string &message) {
	embed(ch, src, 0xff7a7a, message);
}


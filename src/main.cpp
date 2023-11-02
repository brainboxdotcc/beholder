#include <dpp/dpp.h>
#include <dpp/json.h>
#include <fmt/format.h>
#include <beholder/beholder.h>
#include <beholder/listeners.h>
#include <beholder/database.h>
#include <beholder/logger.h>
#include <beholder/config.h>

int main(int argc, char const *argv[])
{
	std::srand(time(NULL));
	config::init("../config.json");
	logger::init(config::get("log"));

	dpp::cluster bot(
		config::get("token"),
		dpp::i_guild_messages | dpp::i_message_content | dpp::i_guild_members,
		1, 0, 1, true, dpp::cache_policy::cpol_none
	);

	bot.on_log(&logger::log);
	bot.on_slashcommand(&listeners::on_slashcommand);
	bot.on_message_create(&listeners::on_message_create);
	bot.on_message_update(&listeners::on_message_update);
	bot.on_ready(&listeners::on_ready);

	db::init(bot);

	/* Start bot */
	bot.set_websocket_protocol(dpp::ws_etf);
	bot.start(dpp::st_wait);
}

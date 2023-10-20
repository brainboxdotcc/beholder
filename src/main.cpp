#include <dpp/dpp.h>
#include <dpp/json.h>
#include <fmt/format.h>
#include <beholder/beholder.h>
#include <beholder/listeners.h>
#include <beholder/database.h>
#include <beholder/logger.h>
#include <beholder/config.h>

std::atomic<int> concurrent_images{0};

int main(int argc, char const *argv[])
{
	config::init();
	logger::init();

	dpp::cluster bot(
		config::get("token"),
		dpp::i_guild_messages | dpp::i_message_content | dpp::i_guild_members,
		1, 0, 1, true, dpp::cache_policy::cpol_none
	);

	logger::log(bot);

	bot.on_slashcommand(&listeners::on_slashcommand);
	bot.on_message_create(&listeners::on_message_create);
	bot.on_ready(&listeners::on_ready);

	/* Connect to SQL database */
	const json& dbconf = config::get("database");
	if (!db::connect(dbconf["host"], dbconf["username"], dbconf["password"], dbconf["database"], dbconf["port"])) {
		bot.log(dpp::ll_critical, fmt::format("Database connection error connecting to {}", dbconf["database"]));
		exit(2);
	}
	bot.log(dpp::ll_info, fmt::format("Connected to database: {}", dbconf["database"]));

	/* Start bot */
	bot.set_websocket_protocol(dpp::ws_etf);
	bot.start(dpp::st_wait);
}

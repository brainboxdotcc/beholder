#include <dpp/dpp.h>
#include <dpp/json.h>
#include <filesystem>
#include <fmt/format.h>
#include <beholder/beholder.h>
#include <beholder/listeners.h>
#include <beholder/database.h>
#include <beholder/logger.h>
#include <beholder/config.h>

#include <beholder/commands/logchannel.h>
#include <beholder/commands/roles.h>
#include <beholder/commands/message.h>
#include <beholder/commands/patterns.h>
#include <beholder/commands/premium.h>
#include <beholder/commands/info.h>

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

	bot.on_slashcommand(&listeners::on_slashcommand);
	bot.on_message_create(&listeners::on_message_create);

	bot.on_ready([&bot](const dpp::ready_t &event) {
		if (dpp::run_once<struct register_bot_commands>()) {
			uint64_t default_permissions = dpp::p_administrator | dpp::p_manage_guild;
			bot.global_bulk_command_create({
				register_command<info_command>(bot),
				register_command<premium_command>(bot),
				register_command<patterns_command>(bot),
				register_command<roles_command>(bot),
				register_command<message_command>(bot),
				register_command<logchannel_command>(bot),
			});

			auto set_presence = [&bot]() {
				bot.current_application_get([&bot](const dpp::confirmation_callback_t& v) {
					if (!v.is_error()) {
						dpp::application app = std::get<dpp::application>(v.value);
						bot.set_presence(dpp::presence(dpp::ps_online, dpp::at_watching, fmt::format("images on {} servers", app.approximate_guild_count)));
					}
				});
			};

			bot.start_timer([&bot, set_presence](dpp::timer t) {
				set_presence();
			}, 240);
			set_presence();
		}
	});

	/* spdlog logger */
	logger::log(bot);

	bot.set_websocket_protocol(dpp::ws_etf);

	/* Connect to SQL database */
	const json& dbconf = config::get("database");
	if (!db::connect(dbconf["host"], dbconf["username"], dbconf["password"], dbconf["database"], dbconf["port"])) {
		bot.log(dpp::ll_critical, fmt::format("Database connection error connecting to {}", dbconf["database"]));
		exit(2);
	}
	bot.log(dpp::ll_info, fmt::format("Connected to database: {}", dbconf["database"]));

	/* This shuts up libleptonica, who tf logs errors to stderr in a lib?! */
	int fd = ::open("/dev/null", O_WRONLY);
	::dup2(fd, 2);
	::close(fd);

	/* Start bot */
	bot.start(dpp::st_wait);
}

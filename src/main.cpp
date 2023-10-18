#include <dpp/dpp.h>
#include <dpp/json.h>
#include <filesystem>
#include <beholder/beholder.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <beholder/listeners.h>
#include <beholder/database.h>

#include <beholder/commands/logchannel.h>
#include <beholder/commands/roles.h>
#include <beholder/commands/message.h>
#include <beholder/commands/patterns.h>
#include <beholder/commands/premium.h>
#include <beholder/commands/info.h>

json configdocument;
std::atomic<int> concurrent_images{0};

int main(int argc, char const *argv[])
{
	/* Set up the bot cluster and read the configuration json */
	std::ifstream configfile("../config.json");
	configfile >> configdocument;
	dpp::cluster bot(configdocument["token"], dpp::i_guild_messages | dpp::i_message_content | dpp::i_guild_members, 1, 0, 1, true, dpp::cache_policy::cpol_none);

	/* Set up spdlog logger */
	std::shared_ptr<spdlog::logger> log;
	spdlog::init_thread_pool(8192, 2);
	std::vector<spdlog::sink_ptr> sinks;
	auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt >();
	auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/beholder.log", 1024 * 1024 * 5, 10);
	sinks.push_back(stdout_sink);
	sinks.push_back(rotating);
	log = std::make_shared<spdlog::async_logger>("file_and_console", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
	spdlog::register_logger(log);
	log->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t]%$ : %v");
	log->set_level(spdlog::level::level_enum::debug);

	/* Integrate all our listeners. */

	bot.on_slashcommand(&command_listener::on_slashcommand);

	/* Handle guild member messages; requires message content intent */
	bot.on_message_create(&message_listener::on_message_create);

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
		}
	});

	/* spdlog logger */
	bot.on_log([&bot, &log](const dpp::log_t & event) {
		switch (event.severity) {
			case dpp::ll_trace:
				log->trace("{}", event.message);
				break;
			case dpp::ll_debug:
				log->debug("{}", event.message);
				break;
			case dpp::ll_info:
				log->info("{}", event.message);
				break;
			case dpp::ll_warning:
				log->warn("{}", event.message);
				break;
			case dpp::ll_error:
				log->error("{}", event.message);
				break;
			case dpp::ll_critical:
			default:
				log->critical("{}", event.message);
				break;
		}
	});

	bot.set_websocket_protocol(dpp::ws_etf);

	/* Connect to SQL database */
	const json& dbconf = configdocument["database"];
	if (!db::connect(dbconf["host"], dbconf["username"], dbconf["password"], dbconf["database"], dbconf["port"])) {
		bot.log(dpp::ll_critical, fmt::format("Database connection error connecting to {}", dbconf["database"]));
		exit(2);
	}
	bot.log(dpp::ll_info, fmt::format("Connected to database: {}", dbconf["database"]));

	/* Start bot */
	bot.start(dpp::st_wait);
}

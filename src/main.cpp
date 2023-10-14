#include <dpp/dpp.h>
#include <dpp/json.h>
#include <filesystem>
#include <yeet/yeet.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <yeet/database.h>

namespace fs = std::filesystem;

json configdocument;
std::atomic<int> concurrent_images{0};

int main(int argc, char const *argv[])
{
	/* Set up the bot cluster and read the configuration json */
	std::ifstream configfile("../config.json");
	configfile >> configdocument;
	dpp::cluster bot(configdocument["token"], dpp::i_default_intents | dpp::i_message_content | dpp::i_guild_members, 1, 0, 1, true, dpp::cache_policy::cpol_none);

	/* Set up spdlog logger */
	std::shared_ptr<spdlog::logger> log;
	spdlog::init_thread_pool(8192, 2);
	std::vector<spdlog::sink_ptr> sinks;
	auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt >();
	auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/yeet.log", 1024 * 1024 * 5, 10);
	sinks.push_back(stdout_sink);
	sinks.push_back(rotating);
	log = std::make_shared<spdlog::async_logger>("test", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
	spdlog::register_logger(log);
	log->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t]%$ : %v");
	log->set_level(spdlog::level::level_enum::debug);

	dpp::commandhandler command_handler(&bot);
	command_handler.add_prefix(".").add_prefix("/");

	/* Todo: command handler here */
	bot.on_ready([&bot](const dpp::ready_t &event) {
	});

	/* Handle guild member messages; requires message content intent */
	bot.on_message_create([&bot](const dpp::message_create_t &ev) {
		auto guild_member = ev.msg.member;
		bool should_bypass = false;

		/* If the author is a bot, stop the event (no checking). */
		if (ev.msg.author.is_bot()) {
			return;
		}

		/* Loop through all bypass roles in database */
		db::resultset bypass_roles = db::query("SELECT * FROM guild_bypass_roles WHERE guild_id = '?'", { ev.msg.guild_id.str() });
		for (const db::row& role : bypass_roles) {
			const auto& roles = guild_member.get_roles();
			if (std::find(roles.begin(), roles.end(), dpp::snowflake(role.at("role_id"))) != roles.end()) {
				/* Stop the event if user is in a bypass role */
				return;
			}
		}

		/* Check each attachment in the message, if any */
		if (ev.msg.attachments.size() > 0) {
			for (const dpp::attachment& attach : ev.msg.attachments) {
				download_image(attach, bot, ev);
			}
		}

		/* Split the message by spaces. */
		std::vector<std::string> parts = dpp::utility::tokenize(ev.msg.content, " ");

		/* Check each word in the message looking for URLs */
		for (const std::string& possibly_url : parts) {
			size_t size = possibly_url.length();
			if ((size >= 9 && dpp::lowercase(possibly_url.substr(0, 8)) == "https://") ||
			    (size >= 8 && dpp::lowercase(possibly_url.substr(0, 7)) == "http://")) {
				dpp::attachment attach((dpp::message*)&ev.msg);
				attach.url = possibly_url;
				/* Strip off query parameters */
				auto pos = possibly_url.find('?');
				if (pos != std::string::npos) {
					possibly_url.substr(0, pos - 1);
				}
				attach.filename = fs::path(possibly_url).filename();
				download_image(attach, bot, ev);
			}
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

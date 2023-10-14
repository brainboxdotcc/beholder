#include <dpp/dpp.h>
#include <dpp/json.h>
#include <filesystem>
#include <yeet/yeet.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <yeet/database.h>

namespace fs = std::filesystem;

json configdocument;
std::atomic<int> concurrent_images{0};

void delete_message_and_warn(dpp::cluster& bot, const dpp::message_create_t ev, const dpp::attachment attach, const std::string text) {
	bot.message_delete(ev.msg.id, ev.msg.channel_id, [&bot, ev, attach, text](const auto& cc) {

		if(cc.is_error()) {
			bad_embed(bot, ev.msg.channel_id, "Failed to delete the message: " + ev.msg.id.str(), ev.msg);
			return;
		}

		db::resultset logchannel = db::query("SELECT log_channel FROM guild_config WHERE guild_id = '?'", { ev.msg.guild_id.str() });
		bad_embed(bot, ev.msg.channel_id, "<@" + ev.msg.author.id.str() + ">, the picture you posted appears to contain program code, program output, or other program related text content. Please **DO NOT** share images of code, paste the code.\n\nIf you continue to send screenshots of code, you may be **muted**.\n\nFor further information, please see rule :regional_indicator_h: of the <#830548236157976617>", ev.msg);
		if (logchannel.size()) {
			good_embed(
				bot, dpp::snowflake(logchannel[0].at("log_channel")), "Attachment: `" + attach.filename + "`\nSent by: `" +
				ev.msg.author.format_username() + "`\nMatched pattern: `" + text + "`\n[Image link](" + attach.url +")"
			);
		}
	});
}

int main(int argc, char const *argv[])
{
	/* Setup the bot */
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

	bot.on_ready([&bot](const dpp::ready_t &event) {
	});

	bot.on_message_create([&bot](const dpp::message_create_t &ev) {
		auto guild_member = ev.msg.member;

		/* Of course, this could be changed to allow more than just a pre-defined role from config. */
		bool should_bypass = false;

		/* Loop through all bypass roles, defined by config. */
		db::resultset bypass_roles = db::query("SELECT * FROM guild_bypass_roles WHERE guild_id = '?'", { ev.msg.guild_id.str() });
		for (const db::row& role : bypass_roles) {
			/* If the user has this role, set should_bypass to true, then kill the loop. */
			const auto& roles = guild_member.get_roles();
			if (std::find(roles.begin(), roles.end(), dpp::snowflake(role.at("role_id"))) != roles.end()) {
				should_bypass = true;
				bot.log(dpp::ll_info, "User " + ev.msg.author.format_username() + " is in exempt role " + role.at("role_id"));
				break;
			}
		}

		/* If the author is a bot, or should bypass this, stop the event (no checking). */
		if (ev.msg.author.is_bot() || should_bypass) {
			return;
		}

		/* Are there any attachments? */
		if (ev.msg.attachments.size() > 0) {
			for (const dpp::attachment& attach : ev.msg.attachments) {
				download_image(attach, bot, ev);
			}
		}

		/* Split the message by spaces. */
		std::vector<std::string> parts = dpp::utility::tokenize(ev.msg.content, " ");

		/* Go through the parts array */
		for (const std::string& possibly_url : parts) {
			size_t size = possibly_url.length();
			if ((size >= 9 && possibly_url.substr(0, 8) == "https://") || (size >= 8 && possibly_url.substr(0, 7) == "http://")) {
				dpp::attachment attach((dpp::message*)&ev.msg);
				attach.url = possibly_url;
				auto pos = possibly_url.find('?');
				if (pos != std::string::npos) {
					possibly_url.substr(0, pos - 1);
				}
				attach.filename = fs::path(possibly_url).filename();
				download_image(attach, bot, ev);
			}
		}
	});

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
	json dbconf = configdocument["database"];
	if (!db::connect(dbconf["host"], dbconf["username"], dbconf["password"], dbconf["database"], dbconf["port"])) {
		std::cerr << "Database connection failed\n";
		exit(2);
	}

	/* Start bot */
	bot.start(dpp::st_wait);
}

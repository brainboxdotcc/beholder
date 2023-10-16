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
	dpp::cluster bot(configdocument["token"], dpp::i_guild_messages | dpp::i_message_content | dpp::i_guild_members, 1, 0, 1, true, dpp::cache_policy::cpol_none);

	/* Set up spdlog logger */
	std::shared_ptr<spdlog::logger> log;
	spdlog::init_thread_pool(8192, 2);
	std::vector<spdlog::sink_ptr> sinks;
	auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt >();
	auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/yeet.log", 1024 * 1024 * 5, 10);
	sinks.push_back(stdout_sink);
	sinks.push_back(rotating);
	log = std::make_shared<spdlog::async_logger>("file_and_console", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
	spdlog::register_logger(log);
	log->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t]%$ : %v");
	log->set_level(spdlog::level::level_enum::debug);

	bot.on_slashcommand([&bot](const dpp::slashcommand_t& event) {
		/* Command to allow bypass roles */
		if (event.command.get_command_name() == "set-roles") {

			/* Create a message */
			dpp::message msg(event.command.channel_id, "Select which roles should bypass image scanning");

			dpp::component select_menu;
			select_menu.set_type(dpp::cot_role_selectmenu)
				.set_min_values(0)
				.set_max_values(25)
				.set_id("add_roles_select_menu");

			/* Loop through all bypass roles in database */
			db::resultset bypass_roles = db::query("SELECT * FROM guild_bypass_roles WHERE guild_id = '?'", { event.command.guild_id });
			for (const db::row& role : bypass_roles) {
				/* Add the role as a default value to the select menu,
				 * letting people know that it's currently a bypass role.
				 */
				select_menu.add_default_value(dpp::snowflake(role.at("role_id")), dpp::cdt_role);
			}

			msg.add_component(dpp::component().add_component(select_menu)).set_flags(dpp::m_ephemeral);

			/* Reply to the user with our message. */
			event.reply(msg);
		}

		if (event.command.get_command_name() == "set-log-channel") {

			/* Create a message */
			dpp::message msg(event.command.channel_id, "Select which channel logs will be sent to");

			dpp::component select_menu;
			select_menu.set_type(dpp::cot_channel_selectmenu)
				.set_min_values(1)
				.set_max_values(1)
				.add_channel_type(dpp::CHANNEL_TEXT)
				.set_id("log_channel_select_menu");

			/* Loop through all bypass roles in database */
			db::resultset channels = db::query("SELECT * FROM guild_config WHERE guild_id = '?'", { event.command.guild_id });
			if (!channels.empty()) {
				/* Add the channel as a default value to the select menu. */
				select_menu.add_default_value(dpp::snowflake(channels[0].at("log_channel")), dpp::cdt_channel);
			}

			msg.add_component(dpp::component().add_component(select_menu)).set_flags(dpp::m_ephemeral);

			/* Reply to the user with our message. */
			event.reply(msg);
		}

		if (event.command.get_command_name() == "set-patterns") {

			std::string patterns;
			db::resultset pattern_query = db::query("SELECT pattern FROM guild_patterns WHERE guild_id = '?' ORDER BY pattern", { event.command.guild_id });
			for (const db::row& p : pattern_query) {
				patterns += p.at("pattern") + "\n";
			}

			dpp::interaction_modal_response modal("set_patterns_modal", "Enter text patterns, one per line");
			/* Add a text component */
			modal.add_component(
				dpp::component()
					.set_label("Text to match in images")
					.set_id("patterns")
					.set_type(dpp::cot_text)
					.set_placeholder("Enter one pattern per line. Images containing the patterns will be deleted.")
					.set_default_value(patterns)
					.set_max_length(4000)
					.set_required(true)
					.set_text_style(dpp::text_paragraph)
			);
			event.dialog(modal, [&bot](const dpp::confirmation_callback_t& cc) {
				if (cc.is_error()) {
					bot.log(dpp::ll_error, cc.http_info.body);
				}
			});
		}

		if (event.command.get_command_name() == "set-delete-message") {

			db::resultset embed = db::query("SELECT embed_body, embed_title FROM guild_config WHERE guild_id = '?'", { event.command.guild_id });
			std::string embed_body, embed_title;
			if (!embed.empty()) {
				embed_body = embed[0].at("embed_body");
				embed_title = embed[0].at("embed_title");
			}

			dpp::interaction_modal_response modal("set_embed_modal", "Enter delete message details");
			/* Add a text component */
			modal.add_component(
				dpp::component()
					.set_label("Message Title")
					.set_id("title")
					.set_type(dpp::cot_text)
					.set_placeholder("Enter the title of the message")
					.set_default_value(embed_title)
					.set_min_length(1)
					.set_max_length(256)
					.set_required(true)
					.set_text_style(dpp::text_short)
			);
			modal.add_row();
			modal.add_component(
				dpp::component()
					.set_label("Message Body")
					.set_id("body")
					.set_type(dpp::cot_text)
					.set_placeholder("Enter body of message. You can use the word '@user' here which will be replaced with a mention of the user who triggered the message.")
					.set_default_value(embed_body)
					.set_min_length(1)
					.set_max_length(3800)
					.set_required(true)
					.set_text_style(dpp::text_paragraph)
			);
			event.dialog(modal);
		}
	});

	bot.on_form_submit([](const dpp::form_submit_t & event) {
		if (event.custom_id == "set_embed_modal") {
			std::string embed_title = std::get<std::string>(event.components[0].components[0].value);
			std::string embed_body = std::get<std::string>(event.components[1].components[0].value);
			db::query(
				"INSERT INTO guild_config (guild_id, embed_title, embed_body) VALUES('?', '?', '?') ON DUPLICATE KEY UPDATE embed_title = '?', embed_body = '?'",
				{ event.command.guild_id.str(), embed_title, embed_body, embed_title, embed_body }
			);
			/* Replace @user with the user's mention for preview */
			embed_body = replace_string(embed_body, "@user", "<@" + event.command.usr.id.str() + ">");
			event.reply(
				dpp::message("✅ Delete message set.\n\n**__Preview:__**")
				.set_flags(dpp::m_ephemeral)
				.add_embed(dpp::embed().set_description(embed_body).set_title(embed_title).set_color(0xff7a7a))
			);
		}
		if (event.custom_id == "set_patterns_modal") {
			std::string pats = std::get<std::string>(event.components[0].components[0].value);
			auto list = dpp::utility::tokenize(pats, "\n");
			db::query("START TRANSACTION");
			db::query("DELETE FROM guild_patterns WHERE guild_id = '?'", { event.command.guild_id.str() });

			if (!db::error().empty()) {
				/* We get out the transaction in the event of a failure. */
				db::query("ROLLBACK");
				event.reply(dpp::message("❌ Failed to set patterns").set_flags(dpp::m_ephemeral));
				return;
			}

			if (!list.empty()) {

				std::string sql_query = "INSERT INTO guild_patterns (guild_id, pattern) VALUES";
				db::paramlist sql_parameters;

				for (std::size_t i = 0; i < list.size(); ++i) {
					sql_query += "(?,'?')";
					if (i != list.size() - 1) {
						sql_query += ",";
					}
					sql_parameters.emplace_back(event.command.guild_id.str());
					sql_parameters.emplace_back(list[i]);
				}

				db::query(sql_query, sql_parameters);

				if (!db::error().empty()) {
					db::query("ROLLBACK");
					event.reply(dpp::message("❌ Failed to set patterns").set_flags(dpp::m_ephemeral));
					return;
				}
			}

			db::query("COMMIT");
			event.reply(dpp::message("✅ " + std::to_string(list.size()) + " Patterns set").set_flags(dpp::m_ephemeral));
		}
	});

	bot.on_select_click([&bot](const dpp::select_click_t & event) {

		if (event.custom_id == "add_roles_select_menu") {

			db::query("START TRANSACTION");
			db::query("DELETE FROM guild_bypass_roles WHERE guild_id = '?'", { event.command.guild_id.str() });

			if (!db::error().empty()) {
				/* We get out the transaction in the event of a failure. */
				db::query("ROLLBACK");
				event.reply(dpp::message("❌ Failed to set new bypass roles").set_flags(dpp::m_ephemeral));
				return;
			}

			if (!event.values.empty()) {

				std::string sql_query = "INSERT INTO guild_bypass_roles (guild_id, role_id) VALUES";
				db::paramlist sql_parameters;

				for (std::size_t i = 0; i < event.values.size(); ++i) {
					sql_query += "(?, ?)";
					if (i != event.values.size() - 1) {
						sql_query += ", ";
					}
					sql_parameters.emplace_back(event.command.guild_id.str());
					sql_parameters.emplace_back(event.values[i]);
				}

				db::query(sql_query, sql_parameters);

				if (!db::error().empty()) {
					db::query("ROLLBACK");
					event.reply(dpp::message("❌ Failed to set new bypass roles").set_flags(dpp::m_ephemeral));
					return;
				}
			}

			db::query("COMMIT");
			event.reply(dpp::message("✅ Bypass roles set").set_flags(dpp::m_ephemeral));
		}

		if (event.custom_id == "log_channel_select_menu") {

			if (event.values.empty()) {
				event.reply(dpp::message("❌ You did not specify a log channel").set_flags(dpp::m_ephemeral));
				return;
			}

			db::query("INSERT INTO guild_config (guild_id, log_channel) VALUES('?', '?') ON DUPLICATE KEY UPDATE log_channel = '?'", { event.command.guild_id.str(), event.values[0], event.values[0] });
			event.reply(dpp::message("✅ Log channel set").set_flags(dpp::m_ephemeral));
		}

	});

	bot.on_ready([&bot](const dpp::ready_t &event) {
		if (dpp::run_once<struct register_bot_commands>()) {
			uint64_t default_permissions = dpp::p_administrator | dpp::p_manage_guild;
			bot.global_bulk_command_create({
				dpp::slashcommand("set-roles", "Set roles that should bypass image scanning", bot.me.id)
					.set_default_permissions(default_permissions),
				dpp::slashcommand("set-log-channel", "Set the channel logs should be sent to", bot.me.id)
					.set_default_permissions(default_permissions),
				dpp::slashcommand("set-delete-message", "Set the details of the embed to send when images are deleted", bot.me.id)
					.set_default_permissions(default_permissions),
				dpp::slashcommand("set-patterns", "Set patterns to find in disallowed images", bot.me.id)
					.set_default_permissions(default_permissions),
			});
		}
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

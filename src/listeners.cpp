#include <fmt/format.h>
#include <beholder/listeners.h>
#include <beholder/database.h>
#include <beholder/beholder.h>
#include <beholder/command.h>

#include <beholder/commands/logchannel.h>
#include <beholder/commands/roles.h>
#include <beholder/commands/message.h>
#include <beholder/commands/patterns.h>
#include <beholder/commands/premium.h>
#include <beholder/commands/info.h>
#include <beholder/commands/ping.h>

namespace listeners {

	void on_ready(const dpp::ready_t &event) {
		dpp::cluster& bot = *event.from->creator;
		if (dpp::run_once<struct register_bot_commands>()) {
			uint64_t default_permissions = dpp::p_administrator | dpp::p_manage_guild;
			bot.global_bulk_command_create({
				register_command<info_command>(bot),
				register_command<premium_command>(bot),
				register_command<patterns_command>(bot),
				register_command<roles_command>(bot),
				register_command<message_command>(bot),
				register_command<logchannel_command>(bot),
				register_command<ping_command>(bot),
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
	}


	void on_slashcommand(const dpp::slashcommand_t &event) {
		route_command(event);
	}

	void on_message_create(const dpp::message_create_t &event) {
		auto guild_member = event.msg.member;
		bool should_bypass = false;

		/* If the author is a bot, stop the event (no checking). */
		if (event.msg.author.is_bot()) {
			return;
		}

		/* Loop through all bypass roles in database */
		db::resultset bypass_roles = db::query("SELECT * FROM guild_bypass_roles WHERE guild_id = '?'", { event.msg.guild_id.str() });
		for (const db::row& role : bypass_roles) {
			const auto& roles = guild_member.get_roles();
			if (std::find(roles.begin(), roles.end(), dpp::snowflake(role.at("role_id"))) != roles.end()) {
				/* Stop the event if user is in a bypass role */
				return;
			}
		}

		/* Check each attachment in the message, if any */
		if (event.msg.attachments.size() > 0) {
			for (const dpp::attachment& attach : event.msg.attachments) {
				download_image(attach, *event.from->creator, event);
			}
		}

		/* Split the message by spaces. */
		std::vector<std::string> parts = dpp::utility::tokenize(event.msg.content, " ");

		/* Check each word in the message looking for URLs */
		for (std::string& possibly_url : parts) {
			size_t size = possibly_url.length();
			if ((size >= 9 && dpp::lowercase(possibly_url.substr(0, 8)) == "https://") ||
			(size >= 8 && dpp::lowercase(possibly_url.substr(0, 7)) == "http://")) {
				dpp::attachment attach((dpp::message*)&event.msg);
				attach.url = possibly_url;
				/* Strip off query parameters */
				auto pos = possibly_url.find('?');
				if (pos != std::string::npos) {
					possibly_url = possibly_url.substr(0, pos);
				}
				attach.filename = fs::path(possibly_url).filename();
				download_image(attach, *event.from->creator, event);
			}
		}
	}
}

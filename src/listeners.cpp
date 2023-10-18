#include <beholder/listeners.h>
#include <beholder/database.h>
#include <beholder/beholder.h>

#include <beholder/commands/logchannel.h>
#include <beholder/commands/roles.h>
#include <beholder/commands/message.h>
#include <beholder/commands/patterns.h>
#include <beholder/commands/premium.h>

namespace command_listener {
	void on_slashcommand(const dpp::slashcommand_t &event) {
		route_command(event);
	}
}

namespace message_listener {
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
		for (const std::string& possibly_url : parts) {
			size_t size = possibly_url.length();
			if ((size >= 9 && dpp::lowercase(possibly_url.substr(0, 8)) == "https://") ||
			(size >= 8 && dpp::lowercase(possibly_url.substr(0, 7)) == "http://")) {
				dpp::attachment attach((dpp::message*)&event.msg);
				attach.url = possibly_url;
				/* Strip off query parameters */
				auto pos = possibly_url.find('?');
				if (pos != std::string::npos) {
					possibly_url.substr(0, pos - 1);
				}
				attach.filename = fs::path(possibly_url).filename();
				download_image(attach, *event.from->creator, event);
			}
		}
	}
}

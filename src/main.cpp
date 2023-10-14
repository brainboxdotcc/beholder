#include <dpp/dpp.h>
#include <dpp/json.h>
#include <filesystem>
#include <yeet/yeet.h>

namespace fs = std::filesystem;

json configdocument;
dpp::snowflake logchannel;
std::atomic<int> concurrent_images{0};

void delete_message_and_warn(dpp::cluster& bot, const dpp::message_create_t ev, const dpp::attachment attach, const std::string text) {
	bot.message_delete(ev.msg.id, ev.msg.channel_id, [&bot, ev, attach, text](const auto& cc) {

		if(cc.is_error()) {
			bad_embed(bot, ev.msg.channel_id, "Failed to delete the message: " + ev.msg.id.str(), ev.msg);
			return;
		}

		bad_embed(bot, ev.msg.channel_id, "<@" + ev.msg.author.id.str() + ">, the picture you posted appears to contain program code, program output, or other program related text content. Please **DO NOT** share images of code, paste the code.\n\nIf you continue to send screenshots of code, you may be **muted**.\n\nFor further information, please see rule :regional_indicator_h: of the <#830548236157976617>", ev.msg);
		good_embed(bot, logchannel, "Attachment: `" + attach.filename + "`\nSent by: `" +
		ev.msg.author.format_username() + "`\nMatched pattern: `" + text + "`\n[Image link](" + attach.url +")");
	});
}

int main(int argc, char const *argv[])
{
	/* Setup the bot */
	std::ifstream configfile("../config.json");
	configfile >> configdocument;
	dpp::cluster bot(configdocument["token"], dpp::i_default_intents | dpp::i_message_content | dpp::i_guild_members, 1, 0, 1, true, dpp::cache_policy::cpol_none);
	dpp::snowflake home_server = dpp::snowflake_not_null(&configdocument, "homeserver");
	logchannel = dpp::snowflake_not_null(&configdocument, "logchannel");

	dpp::commandhandler command_handler(&bot);
	command_handler.add_prefix(".").add_prefix("/");

	bot.on_ready([&bot](const dpp::ready_t &event) {
	});

	bot.on_message_create([&bot, home_server](const dpp::message_create_t &ev) {
		auto guild_member = ev.msg.member;

		/* Of course, this could be changed to allow more than just a pre-defined role from config. */
		bool should_bypass = false;

		/* Loop through all bypass roles, defined by config. */
		for (const std::string& role : configdocument.at("bypassroles")) {
			/* If the user has this role, set should_bypass to true, then kill the loop. */
			const auto& roles = guild_member.get_roles();
			if(std::find(roles.begin(), roles.end(), dpp::snowflake(role)) != roles.end()) {
				should_bypass = true;
				bot.log(dpp::ll_info, "User " + ev.msg.author.format_username() + " is in exempt role " + role);
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

	bot.on_log(dpp::utility::cout_logger());

	bot.set_websocket_protocol(dpp::ws_etf);

	/* Start bot */
	bot.start(dpp::st_wait);
}

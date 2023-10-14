#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

#include <yeet/yeet.h>

using json = dpp::json;

namespace fs = std::filesystem;

constexpr size_t max_size = 8 * 1024 * 1024;
constexpr int max_concurrency = 6;
json configdocument;
dpp::snowflake logchannel;
std::atomic<int> concurrent_images{0};

/**
 * @brief Delete a message and send a warning.
 * @param bot Bot reference.
 * @param ev message_create_t reference.
 * @param attach The attachment that was flagged as bad.
 * @param text What the attachment was flagged for.
 */
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

/**
 * @brief Processes an attachment into text, then checks to see if it matches a certain pattern. If it matches then it called delete_message_and_warn.
 * @param attach The attachment to process into text.
 * @param bot Bot reference.
 * @param ev message_create_t reference.
 */
void process_image(const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
	if (attach.url.find(".webp") != std::string::npos || attach.url.find(".jpg") != std::string::npos || attach.url.find(".png") != std::string::npos || attach.url.find(".gif") != std::string::npos) {
		bot.log(dpp::ll_info, "Image: " + attach.url);
		if (concurrent_images > max_concurrency) {
			bot.log(dpp::ll_info, "Too many concurrent images, skipped");
			return;
		}
		if (attach.width > 4096 || attach.height > 4096) {
			bot.log(dpp::ll_info, "Image dimensions of " + std::to_string(attach.width) + "x" + std::to_string(attach.height) + " too large to be a screenshot");
			return;
		}
		if (attach.size > max_size) {
			bot.log(dpp::ll_info, "Image size of " + std::to_string(attach.size / 1024) + "KB is larger than maximum allowed scanning size");
			return;
		}
		bot.request(attach.url, dpp::m_get, [attach, ev, &bot](const dpp::http_request_completion_t& result) {
			if (result.body.size() > max_size) {
				bot.log(dpp::ll_info, "Image size of " + std::to_string(attach.size / 1024) + "KB is larger than maximum allowed scanning size");
			}
			std::string temp_filename = ev.msg.id.str() + "_" + attach.filename;
			FILE* content = fopen(temp_filename.c_str(), "wb");
			if (!content) {
				bot.log(dpp::ll_info, "Unable to write downloaded file " + temp_filename);
				return;
			}
			fwrite(result.body.data(), result.body.length(), 1, content);
			fclose(content);
			bot.log(dpp::ll_debug, "Downloaded and saved image of size: " + std::to_string(result.body.length()));
			concurrent_images++;
			dpp::utility::exec("../ocr.sh", {temp_filename}, [attach, ev, &bot](std::string output) {
				std::vector<std::string> lines = dpp::utility::tokenize(output, "\n");
				for (const std::string& line : lines) {
					if (line.length() && line[0] == 0x0C) {
						/* Tesseract puts random formdeeds in the output, skip them */
						continue;
					}
					for (const std::string& pattern : configdocument.at("patterns")) {
						std::string pattern_wild = "*" + pattern + "*";
						if (line.length() && pattern.length() && match(line.c_str(), pattern_wild.c_str())) {
							concurrent_images--;
							delete_message_and_warn(bot, ev, attach, pattern);
							return;
						}
					}
					bot.log(dpp::ll_info, "Image content: " + line);
				}
				concurrent_images--;
			});
		});
	}
}


int main(int argc, char const *argv[]) {
	/* Setup the bot */
	std::ifstream configfile("../config.json");
	configfile >> configdocument;
	dpp::cluster bot(configdocument["token"], dpp::i_default_intents | dpp::i_message_content, 1, 0, 1, true, dpp::cache_policy::cpol_none);
	dpp::snowflake home_server = dpp::snowflake_not_null(&configdocument, "homeserver");
	logchannel = dpp::snowflake_not_null(&configdocument, "logchannel");

	dpp::commandhandler command_handler(&bot);
	command_handler.add_prefix(".").add_prefix("/");

	bot.on_ready([&bot](const dpp::ready_t &event) {
	});

	bot.on_message_create([&bot, home_server](const dpp::message_create_t &ev) {
		/* Use guild_get_member to gather the guild member due to cache being disabled. */
		bot.guild_get_member(ev.msg.guild_id, ev.msg.author.id, [&bot, ev](const dpp::confirmation_callback_t& callback) {
			auto guild_member = callback.get<dpp::guild_member>();

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
					process_image(attach, bot, ev);
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
					process_image(attach, bot, ev);
				}
			}
		});
	});

	bot.on_log(dpp::utility::cout_logger());

	bot.set_websocket_protocol(dpp::ws_etf);

	/* Start bot */
	bot.start(dpp::st_wait);
}

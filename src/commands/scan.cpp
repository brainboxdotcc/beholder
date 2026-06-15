/************************************************************************************
 * 
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#include <dpp/dpp.h>
#include <dpp/unicode_emoji.h>
#include <beholder/reactor.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <beholder/commands/scan.h>

dpp::slashcommand scan_command::register_command(dpp::cluster& bot)
{
	dpp::slashcommand c = dpp::slashcommand("scan", "Scan an image", bot.me.id)
		.set_default_permissions(dpp::p_manage_guild);
	c.add_option(dpp::command_option(dpp::co_attachment, "file", "Select an image to scan", true));
	return c;
}

void scan_command::route(const dpp::slashcommand_t& event)
{
	dpp::cluster* bot = event.owner;

	bot->interaction_response_create(
		event.command.id, event.command.token, dpp::interaction_response(
			dpp::ir_deferred_channel_message_with_source,
			dpp::message()
				.set_guild_id(event.command.guild_id)
				.set_channel_id(event.command.channel_id)
				.set_flags(dpp::m_ephemeral)
		)
	);

	dpp::snowflake file_id = std::get<dpp::snowflake>(event.get_parameter("file"));
	dpp::attachment attach = event.command.get_resolved_attachment(file_id);

	dpp::message_create_t msg;
	msg.msg.guild_id = event.command.guild_id;
	msg.msg.channel_id = event.command.channel_id;

	scanner_reactor::instance().submit(attach, *bot, msg, [event, bot, attach](const std::string& hash, const json& scan_response) {
		std::vector<std::string> matches;
		std::vector<std::string> match_names;
		bool is_blocked = false;

		if (scan_response.contains("results") && scan_response.at("results").is_array()) {
			for (const json& result : scan_response.at("results")) {
				std::string scanner_name = "Unknown Scanner";
				std::string result_text = "No match";

				if (result.contains("scanner_name") && result.at("scanner_name").is_string()) {
					scanner_name = result.at("scanner_name").get<std::string>();
				}

				if (result.contains("text") && result.at("text").is_string()) {
					result_text = result.at("text").get<std::string>();
				}

				if (result.contains("blocked") && result.at("blocked").is_boolean() && result.at("blocked").get<bool>()) {
					is_blocked = true;
				}

				matches.emplace_back(result_text);
				match_names.emplace_back(scanner_name);
			}
		} else {
			matches.emplace_back("tessd did not return a scan frame");
			match_names.emplace_back("No scans performed");
		}

		dpp::message msg;
		msg.set_channel_id(event.command.channel_id).add_embed(
			dpp::embed()
				.set_description("**Attachment:** `" + attach.filename + "`\n🔗 [Image link](" + attach.url + ")")
				.set_title("🔍 Image Scanned!")
				.set_color(is_blocked ? colours::bad : colours::good)
				.set_url("https://beholder.cc/")
				.set_footer("Powered by Beholder - Requested by " + std::to_string(event.command.usr.id), bot->me.get_avatar_url())
		).set_flags(dpp::m_ephemeral);

		for (size_t x = 0; x < matches.size(); ++x) {
			msg.embeds[0].add_field(match_names[x], "```\n" + matches[x] + "\n```", true);
		}

		if (!hash.empty()) {
			msg.add_component(dpp::component().add_component(
				dpp::component()
					.set_label(is_blocked ? "Unblock" : "Block")
					.set_type(dpp::cot_button)
					.set_emoji(is_blocked ? dpp::unicode_emoji::white_check_mark : dpp::unicode_emoji::no_entry)
					.set_style(is_blocked ? dpp::cos_success : dpp::cos_danger)
					.set_id(std::string(is_blocked ? "UB;*;" : "BL;*;") + hash)
			));
		}

		bot->log(dpp::ll_info, "Manual scan: " + attach.url);
		INCREMENT_STATISTIC("images_scanned", event.command.guild_id);

		event.edit_response(msg);
	});
}

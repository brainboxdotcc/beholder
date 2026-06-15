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
#include <beholder/proc/spawn.h>
#include <beholder/proc/cpipe.h>
#include <beholder/proc/json_frame.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <beholder/commands/scan.h>
#include <beholder/image.h>

dpp::slashcommand scan_command::register_command(dpp::cluster& bot)
{
	dpp::slashcommand c = dpp::slashcommand("scan", "Scan an image", bot.me.id)
		.set_default_permissions(dpp::p_manage_guild);
	c.add_option(dpp::command_option(dpp::co_attachment, "file", "Select an image to scan", true));
	return c;
}

void scan_command::route(const dpp::slashcommand_t &event)
{
	dpp::cluster* bot = event.owner;


	/* Set 'thinking' state */
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
	std::string hash;
	dpp::attachment attach = event.command.get_resolved_attachment(file_id);
	std::vector<std::string> matches;
	std::vector<std::string> match_names;
	bool is_blocked = false;

	const char* const argv[] = {"./tessd", nullptr};
	spawn tessd(argv);

	proc::write_frame(tessd.stdin, make_fetch_request(attach));

	json hash_response;

	if (!proc::read_frame(tessd.stdout, hash_response)) {
		matches.emplace_back("tessd did not return a hash frame");
		match_names.emplace_back("No scans performed");
	} else if (
		!hash_response.contains("stage")
		|| hash_response.at("stage") != "hash"
		|| !hash_response.contains("hash")
		|| !hash_response.at("hash").is_string()
		) {
		matches.emplace_back("Invalid hash frame returned by tessd");
		match_names.emplace_back("No scans performed");
	} else {
		hash = hash_response.at("hash").get<std::string>();

		db::resultset block_list = db::query(
			"SELECT hash FROM block_list_items WHERE guild_id = ? AND hash = ?",
			{event.command.guild_id, hash}
		);

		if (!block_list.empty()) {
			matches.emplace_back("Image is on the block list");
			match_names.emplace_back("Admin Block List");
			is_blocked = true;
		} else {
			matches.emplace_back("No match");
			match_names.emplace_back("Admin Block List");
		}

		proc::write_frame(
			tessd.stdin,
			make_continue_request(*bot, event.command.guild_id, hash)
		);

		tessd.send_eof();

		json scan_response;

		if (!proc::read_frame(tessd.stdout, scan_response)) {
			matches.emplace_back("tessd did not return a scan frame");
			match_names.emplace_back("No scans performed");
		} else if (
			scan_response.contains("results")
			&& scan_response.at("results").is_array()
			) {
			for (const json& result : scan_response.at("results")) {
				std::string scanner_name = "Unknown Scanner";
				std::string result_text = "No match";

				if (result.contains("scanner_name") && result.at("scanner_name").is_string()) {
					scanner_name = result.at("scanner_name").get<std::string>();
				}

				if (result.contains("text") && result.at("text").is_string()) {
					result_text = result.at("text").get<std::string>();
				}

				if (
					result.contains("blocked")
					&& result.at("blocked").is_boolean()
					&& result.at("blocked").get<bool>()
					) {
					is_blocked = true;
				}

				matches.emplace_back(result_text);
				match_names.emplace_back(scanner_name);
			}
		}

		tessd.wait();
	}

	dpp::message msg;
	msg.set_channel_id(event.command.channel_id).add_embed(
		dpp::embed()
		.set_description("**Attachment:** `" + attach.filename + "`\n🔗 [Image link](" + attach.url +")")
		.set_title("🔍 Image Scanned!")
		.set_color(is_blocked ? colours::bad : colours::good)
		.set_url("https://beholder.cc/")
		.set_footer("Powered by Beholder - Requested by " + std::to_string(event.command.usr.id), bot->me.get_avatar_url())
	).set_flags(dpp::m_ephemeral);
	for (size_t x = 0; x < matches.size(); ++x) {
		msg.embeds[0].add_field(match_names[x], "```\n" + matches[x] + "\n```", true);
	}
	if (!hash.empty()) {
		if (is_blocked) {
			msg.add_component(dpp::component().add_component(
				dpp::component()
					.set_label("Unblock")
					.set_type(dpp::cot_button)
					.set_emoji(dpp::unicode_emoji::white_check_mark)
					.set_style(dpp::cos_success)
					.set_id("UB;*;" + hash)
				)
			);
		} else {
			msg.add_component(dpp::component().add_component(
				dpp::component()
					.set_label("Block")
					.set_type(dpp::cot_button)
					.set_emoji(dpp::unicode_emoji::no_entry)
					.set_style(dpp::cos_danger)
					.set_id("BL;*;" + hash)
				)
			);
		}
	}
	bot->log(dpp::ll_info, "Manual scan: " + attach.url);
	INCREMENT_STATISTIC("images_scanned", event.command.guild_id);

	event.edit_response(msg);
}

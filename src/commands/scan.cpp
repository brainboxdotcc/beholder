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
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <beholder/commands/scan.h>
#include <beholder/image.h>
#include "../3rdparty/httplib.h"
#include <fmt/format.h>
#include <CxxUrl/url.hpp>
#include <exception>

dpp::slashcommand scan_command::register_command(dpp::cluster& bot)
{
	dpp::slashcommand c = dpp::slashcommand("scan", "Scan an image (will use quota)", bot.me.id)
		.set_default_permissions(dpp::p_manage_guild);
	c.add_option(dpp::command_option(dpp::co_attachment, "file", "Select an image to scan", true));
	return c;
}

void scan_command::route(const dpp::slashcommand_t &event)
{
	dpp::cluster* bot = event.from->creator;


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
	dpp::attachment attach = event.command.get_resolved_attachment(file_id);
	std::string file_content, hash;

	Url u(attach.url);
	std::string path = u.path();
	std::string host = u.host();
	std::string scheme = u.scheme();
	host = scheme + "://" + host;
	httplib::Client cli(host.c_str());
	cli.enable_server_certificate_verification(false);
	auto res = cli.Get(u.path());
	if (res) {
		if (res->status < 400) {
			file_content = res->body;
		}
	}

	std::vector<std::string> matches;
	std::vector<std::string> match_names;
	bool flattened = false, is_blocked =false;

	if (!file_content.empty()) {
		hash = sha256(file_content);

		db::resultset block_list = db::query("SELECT hash FROM block_list_items WHERE guild_id = ? AND hash = ?", { event.command.guild_id, hash });
		if (!block_list.empty()) {
			matches.emplace_back("Image is on the block list");
			is_blocked = true;
			INCREMENT_STATISTIC2("images_scanned", "images_blocked", event.command.guild_id);
		} else {
			matches.emplace_back("No match");
		}
		match_names.emplace_back("Admin Block List");

		/* Execute each of the scanners in turn on the image, if any throw, log it */
		size_t n = 0;
		dpp::message_create_t ev(event.from, "");
		ev.msg.author = event.command.usr;
		ev.msg.attachments.emplace_back(attach);
		ev.msg.channel_id = event.command.channel_id;
		ev.msg.guild_id = event.command.guild_id;
		ev.msg.id = 0;

		for (auto& scanner : image::scanners) {
			try {
				if (!(*scanner)(flattened, hash, file_content, attach, *bot, ev, 1, false)) {
					matches.emplace_back("No match or not enabled");
					match_names.emplace_back(image::scanner_names[n]);
				}
			}
			catch (const std::exception &e) {
				matches.emplace_back(e.what());
				match_names.emplace_back(image::scanner_names[n]);
			}
			++n;
		}
	} else {
		matches.emplace_back("Could not download file: " + attach.url);
	}

	dpp::message msg;
	msg.set_channel_id(event.command.channel_id).add_embed(
		dpp::embed()
		.set_description("**Attachment:** `" + attach.filename + "`\n🔗 [Image link](" + attach.url +")")
		.set_title("🔍 Image Scanned!")
		.set_color(matches.size() ? colours::bad : colours::good)
		.set_image("attachment://" + attach.filename)
		.set_url("https://beholder.cc/")
		.set_footer("Powered by Beholder - Requested by " + std::to_string(event.command.usr.id), bot->me.get_avatar_url())
	).add_file(attach.filename, file_content).set_flags(dpp::m_ephemeral);
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
	INCREMENT_STATISTIC("images_scanned", event.command.guild_id);

	event.edit_response(msg);
}

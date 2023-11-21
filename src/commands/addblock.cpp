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
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <beholder/commands/addblock.h>
#include <beholder/database.h>
#include <dpp/dpp.h>
#include <CxxUrl/url.hpp>
#include "../3rdparty/httplib.h"
#include <set>

dpp::slashcommand addblock_command::register_command(dpp::cluster& bot)
{
	bot.on_message_context_menu([&bot](const dpp::message_context_menu_t& event) {
		if (event.command.get_command_name() != "add images to block list") {
			return;
		}

		dpp::message msg = event.ctx_message;
		std::vector<dpp::attachment> attaches;

		/* Set 'thinking' state */
	        bot.interaction_response_create(
			event.command.id, event.command.token, dpp::interaction_response(
				dpp::ir_deferred_channel_message_with_source,
				dpp::message()
				.set_guild_id(event.command.guild_id)
				.set_channel_id(event.command.channel_id)
				.set_flags(dpp::m_ephemeral)
			)
		);

		if (msg.attachments.size() > 0) {
			for (const dpp::attachment& attach : msg.attachments) {
				attaches.emplace_back(attach);
			}
		}

		std::vector<std::string> parts = dpp::utility::tokenize(replace_string(msg.content, "\n", " "), " ");
		if (msg.embeds.size() > 0) {
			for (const dpp::embed& embed : msg.embeds) {
				if (!embed.url.empty()) {
					parts.emplace_back(embed.url);
				}
				if (embed.thumbnail.has_value() && !embed.thumbnail->url.empty()) {
					parts.emplace_back(embed.thumbnail->url);
				}
				if (embed.footer.has_value() && !embed.footer->icon_url.empty()) {
					parts.emplace_back(embed.footer->icon_url);
				}
				if (embed.image.has_value() && !embed.image->url.empty()) {
					parts.emplace_back(embed.image->url);
				}
				if (embed.video.has_value() && !embed.video->url.empty()) {
					parts.emplace_back(embed.video->url);
				}
				if (embed.author.has_value()) {
					if (!embed.author->icon_url.empty()) {
						parts.emplace_back(embed.author->icon_url);
					}
					if (!embed.author->url.empty()) {
						parts.emplace_back(embed.author->url);
					}
				}
				auto spaced = dpp::utility::tokenize(replace_string(embed.description, "\n", " "), " ");
				if (!spaced.empty()) {
					parts.insert(parts.end(), spaced.begin(), spaced.end());
				}
				for (const dpp::embed_field& field : embed.fields) {
					auto spaced = dpp::utility::tokenize(replace_string(field.value, "\n", " "), " ");
					if (!spaced.empty()) {
						parts.insert(parts.end(), spaced.begin(), spaced.end());
					}
				}
			}
		}

		if (msg.stickers.size() > 0) {
			for (const dpp::sticker& sticker : msg.stickers) {
				if (!sticker.id.empty()) {
					parts.emplace_back(sticker.get_url());
				}
			}
		}

		std::set<std::string> checked;
		for (std::string& possibly_url : parts) {
			size_t size = possibly_url.length();
			possibly_url = dpp::lowercase(possibly_url);
			auto cloaked_url_pos = possibly_url.find("](http");
			if (cloaked_url_pos != std::string::npos && possibly_url.length() - cloaked_url_pos - 3 > 7) {
				possibly_url = possibly_url.substr(cloaked_url_pos + 2, possibly_url.length() - cloaked_url_pos - 3);
			}
			if ((size >= 9 && possibly_url.substr(0, 8) == "https://") ||
			(size >= 8 && possibly_url.substr(0, 7) == "http://")) {
				dpp::attachment attach((dpp::message*)&msg);
				attach.url = possibly_url;
				try {
					Url u(possibly_url);
					attach.filename = fs::path(u.path()).filename();
				}
				catch (const std::exception&) {
					attach.filename = fs::path(possibly_url).filename();
				}
				if (checked.find(possibly_url) == checked.end()) {
					attaches.emplace_back(attach);
					checked.emplace(possibly_url);
				}
			}
		}

		size_t added = 0;
		for (const dpp::attachment& attach : attaches) {
			Url url;
			try {
				Url u(attach.url);
				url = u;
			}
			catch (const std::exception&) {
				continue;
			}
			std::string host = url.scheme() + "://" + url.host();
			httplib::Client cli(host.c_str());
			cli.enable_server_certificate_verification(false);
			auto res = cli.Get(url.path());
			if (res) {
				std::string hash = sha256(res->body);
				db::query("INSERT INTO block_list_items (guild_id, hash) VALUES(?,?) ON DUPLICATE KEY UPDATE hash = ?", { event.command.guild_id, hash, hash });
				added++;
			}
		}
		if (added == 1) {
			event.edit_response(dpp::message(event.command.channel_id, ":no_entry: **" + std::to_string(added) + "** image has been **added to the block list**. It will be **instantly deleted** without performing any further checks.").set_flags(dpp::m_ephemeral));
		} else {
			event.edit_response(dpp::message(event.command.channel_id, ":no_entry: **" + std::to_string(added) + "** images have been **added to the block list**. They will be **instantly deleted** without performing any further checks.").set_flags(dpp::m_ephemeral));
		}
	});

	return dpp::slashcommand("Add images to block list", "Add any images found in this mesage to the block list", bot.me.id)
	.set_type(dpp::ctxm_message)
	.set_default_permissions(dpp::p_manage_guild);
}

void addblock_command::route(const dpp::slashcommand_t &event)
{
}

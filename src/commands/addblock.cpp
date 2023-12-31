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
#include <beholder/image_iterator.h>
#include <dpp/dpp.h>

dpp::slashcommand addblock_command::register_command(dpp::cluster& bot)
{
	bot.on_message_context_menu([&bot](const dpp::message_context_menu_t& event) {
		if (event.command.get_command_name() != "add images to block list") {
			return;
		}

		dpp::message msg = event.ctx_message;

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

		dpp::permission p = event.command.get_resolved_permission(event.command.usr.id);
		if (!p.has(dpp::p_manage_guild)) {
			event.edit_response(dpp::message(event.command.channel_id, "You require the manage server permission to add images to the block list.").set_flags(dpp::m_ephemeral));
			return;
		}

		image::iterate(msg, event, [&bot, event](std::string url, std::string hash, dpp::message_context_menu_t ev) -> bool {
			db::query("INSERT INTO block_list_items (guild_id, hash) VALUES(?,?) ON DUPLICATE KEY UPDATE hash = ?", { event.command.guild_id, hash, hash });
			return false;
		}, [&bot, event](std::vector<std::string> images, size_t count) {
			if (count == 0) {
				event.edit_response(dpp::message(event.command.channel_id, "No images or stickers found in this message.").set_flags(dpp::m_ephemeral));
			} else if (count == 1) {
				event.edit_response(dpp::message(event.command.channel_id, ":no_entry: **" + std::to_string(count) + "** image has been **added to the block list**. It will be **instantly deleted** without performing any further checks.").set_flags(dpp::m_ephemeral));
			} else {
				event.edit_response(dpp::message(event.command.channel_id, ":no_entry: **" + std::to_string(count) + "** images have been **added to the block list**. They will be **instantly deleted** without performing any further checks.").set_flags(dpp::m_ephemeral));
			}
		});

	});

	return dpp::slashcommand("Add images to block list", "Add any images found in this mesage to the block list", bot.me.id)
		.set_type(dpp::ctxm_message)
		.set_default_permissions(dpp::p_manage_guild);
}

void addblock_command::route(const dpp::slashcommand_t &event)
{
}

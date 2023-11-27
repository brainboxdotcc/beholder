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
#include <beholder/commands/showtags.h>
#include <beholder/image_iterator.h>
#include <dpp/dpp.h>
#include <fmt/format.h>

std::string ucwords(std::string text) {

	for (size_t x = 0; x < text.length(); x++){
		if (x == 0) {
			text[x] = toupper(text[x]);
		} else if (text[x - 1] == ' ') {
			text[x] = toupper(text[x]);
		} else {
			text[x] = tolower(text[x]);
		}
	}
	return text;
}

dpp::slashcommand showtags_command::register_command(dpp::cluster& bot)
{
	bot.on_message_context_menu([&bot](const dpp::message_context_menu_t& event) {
		if (event.command.get_command_name() != "show labels") {
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

		dpp::embed e;
		e.set_description("**__List of labels for images__**")
			.set_title("Label List")
			.set_color(colours::good)
			.set_url("https://beholder.cc/")
			.set_footer("Powered by Beholder - Message ID " + std::to_string(msg.id), bot.me.get_avatar_url());
		
		image::iterate(msg, event, [&bot, event, &e](std::string url, std::string hash, dpp::message_context_menu_t ev) -> bool {
			const auto rs = db::query("SELECT label, confidence FROM vw_label_confidence WHERE hash = ?", { hash });
			for (const auto& row : rs) {
				e.add_field(ucwords(row.at("label")), fmt::format("{0:.02f}{1}", atof(row.at("confidence").c_str()), '%'));
			}
			return false;
		}, [&bot, event, &e](std::vector<std::string> images, size_t count) {
			if (count == 0) {
				event.edit_response(dpp::message(event.command.channel_id, "No images or stickers found in this message.").set_flags(dpp::m_ephemeral));
			} else {
				event.edit_response(dpp::message(event.command.channel_id, "").add_embed(e).set_flags(dpp::m_ephemeral));
			}

		});
	});

	return dpp::slashcommand("Show labels", "Show AI labels for any images in the message", bot.me.id)
		.set_type(dpp::ctxm_message)
		.set_default_permissions(dpp::p_manage_guild);
}

void showtags_command::route(const dpp::slashcommand_t &event)
{
}

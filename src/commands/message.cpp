/************************************************************************************
 * 
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023,2026 Craig Edwards <support@sporks.gg>
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
#include <beholder/commands/message.h>

dpp::slashcommand message_command::register_command(dpp::cluster& bot)
{
	bot.on_form_submit([&bot](const dpp::form_submit_t& event) {
		if (event.custom_id == "set_embed_modal") {
			if (event.components.size() < 2 || !std::holds_alternative<std::string>(event.components[0].value) || !std::holds_alternative<std::string>(event.components[1].value)) {
				event.reply(dpp::message("❌ Invalid modal response").set_flags(dpp::m_ephemeral));
				return;
			}
			std::string embed_title = std::get<std::string>(event.components[0].value);
			std::string embed_body = std::get<std::string>(event.components[1].value);
			db::query(
				"INSERT INTO guild_config (guild_id, embed_title, embed_body) VALUES(?, ?, ?) ON DUPLICATE KEY UPDATE embed_title = ?, embed_body = ?",
				{ event.command.guild_id, embed_title, embed_body, embed_title, embed_body }
			);
			/* Replace @user with the user's mention for preview */
			embed_body = replace_string(embed_body, "@user", "<@" + event.command.usr.id.str() + ">");
			event.reply(
				dpp::message("✅ Delete message set.\n\n**__Preview:__**")
					.set_flags(dpp::m_ephemeral)
					.add_embed(dpp::embed().set_description(embed_body).set_title(embed_title).set_color(0xff7a7a))
			);
		}
	});

	return dpp::slashcommand("message", "Set the details of the message to send when images have forbidden text", bot.me.id)
		.add_option(dpp::command_option(dpp::co_sub_command, "content", "Set the message content and title"))
		.add_option(
			dpp::command_option(dpp::co_sub_command, "enable", "Enable sending messages when deleting images")
				.add_option(
					dpp::command_option(dpp::co_channel, "channel", "Only enable messages in a specific channel", false)
						.add_channel_type(dpp::CHANNEL_TEXT)
				)
		)
		.add_option(
			dpp::command_option(dpp::co_sub_command, "disable", "Disable sending messages when deleting images")
				.add_option(
					dpp::command_option(dpp::co_channel, "channel", "Only disable messages in a specific channel", false)
						.add_channel_type(dpp::CHANNEL_TEXT)
				)
		)
		.set_default_permissions(dpp::p_manage_guild);
}


void message_command::route(const dpp::slashcommand_t &event)
{
	dpp::command_interaction cmd_data = event.command.get_command_interaction();
	auto subcommand = cmd_data.options[0];

	if (subcommand.name == "content") {

		db::resultset embed = db::query("SELECT embed_body, embed_title FROM guild_config WHERE guild_id = ?", { event.command.guild_id });
		std::string embed_body, embed_title;
		if (!embed.empty()) {
			embed_body = embed[0].at("embed_body");
			embed_title = embed[0].at("embed_title");
		}

		dpp::interaction_modal_response modal("set_embed_modal", "Enter delete message details");
		/* Add a text component */
		modal.add_component(
			dpp::component()
				.set_label("Message Title")
				.set_id("title")
				.set_type(dpp::cot_text)
				.set_placeholder("Enter the title of the message")
				.set_default_value(embed_title)
				.set_min_length(1)
				.set_max_length(256)
				.set_required(true)
				.set_text_style(dpp::text_short)
		);
		modal.add_row();
		modal.add_component(
			dpp::component()
				.set_label("Message Body")
				.set_id("body")
				.set_type(dpp::cot_text)
				.set_placeholder("Enter body of message. You can use the word '@user' here which will be replaced with a mention of the user who triggered the message.")
				.set_default_value(embed_body)
				.set_min_length(1)
				.set_max_length(3800)
				.set_required(true)
				.set_text_style(dpp::text_paragraph)
		);
		event.dialog(modal);

	} else if (subcommand.name == "enable" || subcommand.name == "disable") {
		dpp::command_value channel_param = event.get_parameter("channel");
		bool has_channel = std::holds_alternative<dpp::snowflake>(channel_param);
		dpp::snowflake channel_id = has_channel ? std::get<dpp::snowflake>(channel_param) : dpp::snowflake(0);
		bool warn = subcommand.name == "enable";

		db::query(
			"INSERT INTO channel_settings (guild_id, channel_id, warn) VALUES(?, ?, ?) ON DUPLICATE KEY UPDATE warn = ?",
			{ event.command.guild_id, channel_id, warn, warn }
		);

		if (!db::error().empty()) {
			event.reply(dpp::message("❌ Failed to update message settings").set_flags(dpp::m_ephemeral));
			return;
		}

		if (warn) {
			event.reply(
				dpp::message(
					has_channel
					? "✅ Messages __**will be sent**__ in the selected channel when Beholder deletes images."
					: "✅ Messages __**will be sent**__ in channels where Beholder deletes images."
				).set_flags(dpp::m_ephemeral)
			);
		} else {
			event.reply(
				dpp::message(
					has_channel
					? "✅ Messages __**will not be sent**__ in the selected channel when Beholder deletes images.\nBeholder will still send audit log entries, if configured."
					: "✅ Messages __**will not be sent**__ in channels where Beholder deletes images.\nBeholder will still send audit log entries, if configured."
				).set_flags(dpp::m_ephemeral)
			);
		}
	}
}
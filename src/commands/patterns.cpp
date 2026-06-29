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
#include <beholder/commands/patterns.h>
#include <beholder/database.h>

dpp::slashcommand patterns_command::register_command(dpp::cluster& bot)
{
	return dpp::slashcommand("pattern", "Manage OCR patterns to find in disallowed images", bot.me.id)
		.add_option(
			dpp::command_option(dpp::co_sub_command, "add", "Add a pattern")
				.add_option(dpp::command_option(dpp::co_string, "pattern", "Text pattern to match", true))
				.add_option(
					dpp::command_option(dpp::co_channel, "channel", "Only apply this pattern to a specific channel", false)
						.add_channel_type(dpp::CHANNEL_TEXT)
				)
		)
		.add_option(
			dpp::command_option(dpp::co_sub_command, "delete", "Delete a pattern")
				.add_option(dpp::command_option(dpp::co_string, "pattern", "Text pattern to delete", true))
				.add_option(
					dpp::command_option(dpp::co_channel, "channel", "Delete the pattern from a specific channel", false)
						.add_channel_type(dpp::CHANNEL_TEXT)
				)
		)
		.set_default_permissions(dpp::p_manage_guild);
}


dpp::task<void> patterns_command::route(const dpp::slashcommand_t &event)
{
	const std::string action = event.command.get_command_interaction().options[0].name;

	std::string pattern = std::get<std::string>(event.get_parameter("pattern")).substr(0, 150);

	dpp::command_value channel_param = event.get_parameter("channel");
	bool has_channel = std::holds_alternative<dpp::snowflake>(channel_param);

	if (action == "add") {
		if (has_channel) {
			dpp::snowflake channel_id = std::get<dpp::snowflake>(channel_param);
			co_await db::co_query(
				"INSERT INTO guild_patterns (guild_id, channel_id, pattern) VALUES(?, ?, ?)",
				{ event.command.guild_id, channel_id, pattern }
			);
		} else {
			co_await db::co_query(
				"INSERT INTO guild_patterns (guild_id, pattern) VALUES(?, ?)",
				{ event.command.guild_id, pattern }
			);
		}

		if (!db::error().empty()) {
			event.reply(dpp::message("❌ Failed to add pattern").set_flags(dpp::m_ephemeral));
			co_return;
		}

		co_await event.co_reply(dpp::message("✅ Pattern added").set_flags(dpp::m_ephemeral));
		co_return;
	}

	if (action == "delete") {
		if (has_channel) {
			dpp::snowflake channel_id = std::get<dpp::snowflake>(channel_param);
			co_await db::co_query(
				"DELETE FROM guild_patterns WHERE guild_id = ? AND channel_id = ? AND pattern = ?",
				{ event.command.guild_id, channel_id, pattern }
			);
		} else {
			co_await db::co_query(
				"DELETE FROM guild_patterns WHERE guild_id = ? AND channel_id IS NULL AND pattern = ?",
				{ event.command.guild_id, pattern }
			);
		}

		if (!db::error().empty()) {
			co_await event.co_reply(dpp::message("❌ Failed to delete pattern").set_flags(dpp::m_ephemeral));
			co_return;
		}

		co_await event.co_reply(dpp::message("✅ Pattern deleted").set_flags(dpp::m_ephemeral));
	}
}

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
#include <beholder/commands/logchannel.h>
#include <beholder/database.h>

dpp::slashcommand logchannel_command::register_command(dpp::cluster& bot)
{
	bot.on_select_click([&bot](const dpp::select_click_t& event) {
		if (event.custom_id == "log_channel_select_menu") {

			if (event.values.empty()) {
				event.reply(dpp::message("❌ You did not specify a log channel").set_flags(dpp::m_ephemeral));
				return;
			}

			db::query("INSERT INTO guild_config (guild_id, log_channel) VALUES(?, ?) ON DUPLICATE KEY UPDATE log_channel = ?", { event.command.guild_id.str(), event.values[0], event.values[0] });
			event.reply(dpp::message("✅ Log channel set").set_flags(dpp::m_ephemeral));
		}
	});

	return dpp::slashcommand("logchannel", "Set the channel logs should be sent to", bot.me.id)
		.set_default_permissions(dpp::p_administrator | dpp::p_manage_guild);
}


void logchannel_command::route(const dpp::slashcommand_t &event)
{
	/* Create a message */
	dpp::message msg(event.command.channel_id, "Select which channel logs will be sent to");

	dpp::component select_menu;
	select_menu.set_type(dpp::cot_channel_selectmenu)
		.set_min_values(1)
		.set_max_values(1)
		.add_channel_type(dpp::CHANNEL_TEXT)
		.set_id("log_channel_select_menu");

	/* Loop through all bypass roles in database */
	db::resultset channels = db::query("SELECT * FROM guild_config WHERE guild_id = ?", { event.command.guild_id });
	if (!channels.empty()) {
		/* Add the channel as a default value to the select menu. */
		select_menu.add_default_value(dpp::snowflake(channels[0].at("log_channel")), dpp::cdt_channel);
	}

	msg.add_component(dpp::component().add_component(select_menu)).set_flags(dpp::m_ephemeral);

	/* Reply to the user with our message. */
	event.reply(msg);
}
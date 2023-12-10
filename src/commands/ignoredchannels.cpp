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
#include <beholder/commands/ignoredchannels.h>
#include <beholder/database.h>

dpp::slashcommand ignoredchannels_command::register_command(dpp::cluster& bot)
{
	bot.on_select_click([&bot](const dpp::select_click_t& event) {
		if ((event.custom_id == "ignore_add_select_menu" || event.custom_id == "ignore_del_select_menu") && event.values.empty()) {
			event.reply(dpp::message("❌ You did not specify a channel").set_flags(dpp::m_ephemeral));
			return;
		}
		if (event.custom_id == "ignore_add_select_menu") {
			db::query("INSERT INTO guild_ignored_channels (guild_id, channel_id) VALUES(?, ?) ON DUPLICATE KEY UPDATE channel_id = ?", { event.command.guild_id, event.values[0], event.values[0] });
			event.reply(dpp::message("✅ Ignored channel <#" + event.values[0] + ">").set_flags(dpp::m_ephemeral));
		} else if (event.custom_id == "ignore_del_select_menu") {
			db::query("DELETE FROM guild_ignored_channels WHERE guild_id = ? AND channel_id = ?", { event.command.guild_id, event.values[0] });
			if (db::affected_rows() == 1) {
				event.reply(dpp::message("✅ No longer ignoring channel <#" + event.values[0] + ">").set_flags(dpp::m_ephemeral));
			} else {
				event.reply(dpp::message("❌ Channel <#" + event.values[0] + "> was not on the ignore list").set_flags(dpp::m_ephemeral));
			}
		}
	});

	return dpp::slashcommand("ignoredchannels", "Manage channels beholder will ignore", bot.me.id)
		.add_option(dpp::command_option(dpp::co_sub_command, "add", "Add an ignord channel"))
		.add_option(dpp::command_option(dpp::co_sub_command, "delete", "Remove an ignored channel"))
		.add_option(dpp::command_option(dpp::co_sub_command, "list", "List an ignored channel"))
		.set_default_permissions(dpp::p_manage_guild);
}


void ignoredchannels_command::route(const dpp::slashcommand_t &event)
{
	dpp::command_interaction cmd_data = event.command.get_command_interaction();
	auto subcommand = cmd_data.options[0];

	dpp::component select_menu;
	select_menu.set_type(dpp::cot_channel_selectmenu)
		.set_min_values(1)
		.set_max_values(1)
		.add_channel_type(dpp::CHANNEL_TEXT);

	if (subcommand.name == "add") {
		dpp::message msg(event.command.channel_id, "Select a channel which beholder will ignore.\nYou can repeatedly select from the list below to add multiple channels.");
		select_menu.set_id("ignore_add_select_menu");
		msg.add_component(dpp::component().add_component(select_menu)).set_flags(dpp::m_ephemeral);
		event.reply(msg);
	} else if (subcommand.name == "delete") {
		dpp::message msg(event.command.channel_id, "Select a channel which beholder will no longer ignore.\nYou can repeatedly select from the list below to delete multiple channels.");
		select_menu.set_id("ignore_del_select_menu");
		msg.add_component(dpp::component().add_component(select_menu)).set_flags(dpp::m_ephemeral);
		event.reply(msg);
	} else {
		std::string body;
		db::resultset channels = db::query("SELECT channel_id FROM guild_ignored_channels WHERE guild_id = ?", { event.command.guild_id });
		if (channels.size() == 0) {
			body = "No channels are currently ignored";
		} else {
			for (const auto& channel : channels) {
				body += "<#" + channel.at("channel_id") +">\n";
			}
		}
		dpp::embed embed = dpp::embed()
			.set_url("https://beholder.cc/")
			.set_title("Beholder Ignored Channel List")
			.set_footer(dpp::embed_footer{ 
				.text = "Requested by " + event.command.usr.format_username(), 
				.icon_url = event.from->creator->me.get_avatar_url(), 
				.proxy_url = "",
			})
			.set_colour(0x7aff7a)
			.set_description(body);
		event.reply(dpp::message().add_embed(embed));
	}
}
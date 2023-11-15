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
#include <beholder/commands/premium.h>
#include <beholder/database.h>

dpp::slashcommand premium_command::register_command(dpp::cluster& bot)
{
	bot.on_form_submit([&bot](const dpp::form_submit_t& event) {
		if (event.custom_id == "set_premium_embed_modal") {
			std::string embed_title = std::get<std::string>(event.components[0].components[0].value);
			std::string embed_body = std::get<std::string>(event.components[1].components[0].value);
			db::query(
				"INSERT INTO guild_config (guild_id, premium_title, premium_body) VALUES(?, ?, ?) ON DUPLICATE KEY UPDATE premium_title = ?, premium_body = ?",
				{ event.command.guild_id.str(), embed_title, embed_body, embed_title, embed_body }
			);
			/* Replace @user with the user's mention for preview */
			embed_body = replace_string(embed_body, "@user", "<@" + event.command.usr.id.str() + ">");
			event.reply(
				dpp::message("✅ Premium image recognition message set.\n\n**__Preview:__**")
					.set_flags(dpp::m_ephemeral)
					.add_embed(dpp::embed().set_description(embed_body).set_title(embed_title).set_color(0xff7a7a))
			);
		}
	});

	bot.on_select_click([&bot](const dpp::select_click_t& event) {
		if (event.custom_id == "premium_patterns_select_menu") {
			db::transaction();
			db::query("DELETE FROM premium_filters WHERE guild_id = ?", { event.command.guild_id.str() });

			if (!db::error().empty()) {
				/* We get out the transaction in the event of a failure. */
				db::rollback();
				event.reply(dpp::message("❌ Failed to set new image recognition filters").set_flags(dpp::m_ephemeral));
				return;
			}

			if (!event.values.empty()) {

				std::string sql_query = "INSERT INTO premium_filters (guild_id, pattern, score) VALUES";
				db::paramlist sql_parameters;

				for (std::size_t i = 0; i < event.values.size(); ++i) {
					sql_query += "(?, ?, 0.7)";
					if (i != event.values.size() - 1) {
						sql_query += ", ";
					}
					sql_parameters.emplace_back(event.command.guild_id.str());
					sql_parameters.emplace_back(event.values[i]);
				}

				db::query(sql_query, sql_parameters);

				if (!db::error().empty()) {
					db::rollback();
					event.reply(dpp::message("❌ Failed to set new image recognition filters").set_flags(dpp::m_ephemeral));
					return;
				}
			}

			db::commit();
			event.reply(dpp::message("✅ Image recogntition filters set").set_flags(dpp::m_ephemeral));
		}
	});

	return dpp::slashcommand("premium", "Premium bot features", bot.me.id)
		.add_option(dpp::command_option(dpp::co_sub_command, "message", "Set embed to send for image recognition detection (premium only)"))
		.add_option(dpp::command_option(dpp::co_sub_command, "patterns", "Set image recognition categories to block (premium only)"))
		.set_default_permissions(dpp::p_manage_guild);
}


void premium_command::route(const dpp::slashcommand_t &event)
{
	dpp::command_interaction cmd_data = event.command.get_command_interaction();
	auto subcommand = cmd_data.options[0];
	db::resultset embed = db::query("SELECT premium_subscription, premium_body, premium_title FROM guild_config WHERE guild_id = ?", { event.command.guild_id.str() });
	if (embed.empty() || embed[0].at("premium_subscription").empty()) {
		event.reply("You don't appear to be a Beholder Premium subscriber!");
	}

	if (subcommand.name == "message") { // premium

		std::string embed_body, embed_title;
		if (!embed.empty()) {
			embed_body = embed[0].at("premium_body");
			embed_title = embed[0].at("premium_title");
		}

		dpp::interaction_modal_response modal("set_premium_embed_modal", "Enter image recognition message details");
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
	}
	if (subcommand.name == "patterns") { // premium

		dpp::message msg(event.command.channel_id, "Set the message displayed when images are blocked due to premium filters");

		dpp::component select_menu;
		select_menu.set_type(dpp::cot_selectmenu)
			.set_min_values(0)
			.set_max_values(25)
			.set_id("premium_patterns_select_menu");

		db::resultset rows = db::query("SELECT * FROM premium_filter_model LEFT JOIN premium_filters ON guild_id = ? AND category = pattern", { event.command.guild_id.str() });
		for (const auto& row : rows) {
			auto opt = dpp::select_option(row.at("description"), row.at("category"), row.at("detail"));
			if (!row.at("pattern").empty()) {
				opt.set_default(true);
			}
			select_menu.add_select_option(opt);
		}

		msg.add_component(dpp::component().add_component(select_menu)).set_flags(dpp::m_ephemeral);

		/* Reply to the user with our message. */
		event.reply(msg);

	}
}
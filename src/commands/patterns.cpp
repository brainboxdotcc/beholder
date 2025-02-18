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
#include <beholder/commands/patterns.h>
#include <beholder/database.h>

dpp::slashcommand patterns_command::register_command(dpp::cluster& bot)
{
	bot.on_form_submit([&bot](const dpp::form_submit_t& event) {
		if (event.custom_id == "set_patterns_modal") {
			std::string pats = std::get<std::string>(event.components[0].components[0].value);
			auto list = dpp::utility::tokenize(pats, "\n");
			db::transaction();
			db::query("DELETE FROM guild_patterns WHERE guild_id = ?", { event.command.guild_id });

			if (!db::error().empty()) {
				/* We get out the transaction in the event of a failure. */
				db::rollback();
				event.reply(dpp::message("❌ Failed to set patterns").set_flags(dpp::m_ephemeral));
				return;
			}

			if (!list.empty()) {

				std::string sql_query = "INSERT INTO guild_patterns (guild_id, pattern) VALUES";
				db::paramlist sql_parameters;

				for (std::size_t i = 0; i < list.size(); ++i) {
					sql_query += "(?,?)";
					if (i != list.size() - 1) {
						sql_query += ",";
					}
					sql_parameters.emplace_back(event.command.guild_id);
					sql_parameters.emplace_back(list[i].substr(0, 150));
				}

				db::query(sql_query, sql_parameters);

				if (!db::error().empty()) {
					db::rollback();
					event.reply(dpp::message("❌ Failed to set patterns").set_flags(dpp::m_ephemeral));
					return;
				}
			}

			db::commit();
			event.reply(dpp::message("✅ " + std::to_string(list.size()) + " Patterns set").set_flags(dpp::m_ephemeral));
		}
	});

	return dpp::slashcommand("patterns", "Set patterns to find in disallowed images", bot.me.id)
		.set_default_permissions(dpp::p_manage_guild);
}


void patterns_command::route(const dpp::slashcommand_t &event)
{
	std::string patterns;
	db::resultset pattern_query = db::query("SELECT pattern FROM guild_patterns WHERE guild_id = ? ORDER BY pattern", { event.command.guild_id });
	for (const db::row& p : pattern_query) {
		patterns += p.at("pattern") + "\n";
	}

	dpp::interaction_modal_response modal("set_patterns_modal", "Enter text patterns, one per line");
	/* Add a text component */
	modal.add_component(
		dpp::component()
			.set_label("Text to match in images")
			.set_id("patterns" + std::to_string(time(nullptr)))
			.set_type(dpp::cot_text)
			.set_placeholder("Enter one pattern per line. Images containing the patterns will be deleted.")
			.set_max_length(4000)
			.set_required(true)
			.set_text_style(dpp::text_paragraph)
	);
	if (!patterns.empty()) {
		modal.components[0][0].set_default_value(patterns);
	}
	event.dialog(modal, [event](const dpp::confirmation_callback_t& cc) {
		if (cc.is_error()) {
			event.owner->log(dpp::ll_error, cc.http_info.body);
		}
	});
}
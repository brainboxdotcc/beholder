#include <beholder/beholder.h>
#include <beholder/database.h>
#include <beholder/commands/roles.h>
#include <beholder/database.h>

dpp::slashcommand roles_command::register_command(dpp::cluster& bot)
{
	bot.on_select_click([&bot](const dpp::select_click_t& event) {
		if (event.custom_id == "add_roles_select_menu") {

			db::query("START TRANSACTION");
			db::query("DELETE FROM guild_bypass_roles WHERE guild_id = '?'", { event.command.guild_id.str() });

			if (!db::error().empty()) {
				/* We get out the transaction in the event of a failure. */
				db::query("ROLLBACK");
				event.reply(dpp::message("❌ Failed to set new bypass roles").set_flags(dpp::m_ephemeral));
				return;
			}

			if (!event.values.empty()) {

				std::string sql_query = "INSERT INTO guild_bypass_roles (guild_id, role_id) VALUES";
				db::paramlist sql_parameters;

				for (std::size_t i = 0; i < event.values.size(); ++i) {
					sql_query += "(?, ?)";
					if (i != event.values.size() - 1) {
						sql_query += ", ";
					}
					sql_parameters.emplace_back(event.command.guild_id.str());
					sql_parameters.emplace_back(event.values[i]);
				}

				db::query(sql_query, sql_parameters);

				if (!db::error().empty()) {
					db::query("ROLLBACK");
					event.reply(dpp::message("❌ Failed to set new bypass roles").set_flags(dpp::m_ephemeral));
					return;
				}
			}

			db::query("COMMIT");
			event.reply(dpp::message("✅ Bypass roles set").set_flags(dpp::m_ephemeral));
		}
	});

	return dpp::slashcommand("roles", "Set roles that should bypass image scanning", bot.me.id)
		.set_default_permissions(dpp::p_administrator | dpp::p_manage_guild);
}


void roles_command::route(const dpp::slashcommand_t &event)
{
	/* Create a message */
	dpp::message msg(event.command.channel_id, "Select which roles should bypass image scanning");

	dpp::component select_menu;
	select_menu.set_type(dpp::cot_role_selectmenu)
		.set_min_values(0)
		.set_max_values(25)
		.set_id("add_roles_select_menu");

	/* Loop through all bypass roles in database */
	db::resultset bypass_roles = db::query("SELECT * FROM guild_bypass_roles WHERE guild_id = '?'", { event.command.guild_id });
	for (const db::row& role : bypass_roles) {
		/* Add the role as a default value to the select menu,
			* letting people know that it's currently a bypass role.
			*/
		select_menu.add_default_value(dpp::snowflake(role.at("role_id")), dpp::cdt_role);
	}

	msg.add_component(dpp::component().add_component(select_menu)).set_flags(dpp::m_ephemeral);

	/* Reply to the user with our message. */
	event.reply(msg);
}
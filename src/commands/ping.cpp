#include <beholder/beholder.h>
#include <beholder/database.h>
#include <beholder/commands/ping.h>
#include <beholder/database.h>

dpp::slashcommand ping_command::register_command(dpp::cluster& bot)
{
	return dpp::slashcommand("ping", "Show bot latency statistics", bot.me.id);
}

void ping_command::route(const dpp::slashcommand_t &event)
{
	dpp::cluster* bot = event.from->creator;

	dpp::embed embed;

	embed = dpp::embed()
		.set_url("https://beholder.cc/")
		.set_title("Beholder - Ping")
		.set_colour(0x7aff7a)
		.set_thumbnail(bot->me.get_default_avatar_url())
		.set_description(":ping_pong: Ping information:");

	/* Get the rest ping from discord. */
	double discord_api_ping = bot->rest_ping * 1000;

	embed.add_field("Rest ping", std::to_string(discord_api_ping), true);

	/* Calculate the DB time */
	double start = dpp::utility::time_f();
	db::resultset q = db::query("SHOW TABLES", {});

	/* We don't ever want to show ping if the db failed to connect.
	 * To make sure of this, we'll make sure the error field is empty, if it isn't then we'll state that the db failed.
	 */
	if(!db::error().empty()) {
		embed.add_field("DB ping", ":warning: Failed to communicate with database. Report this ASAP.", true);
	} else {
		double db_ping = (dpp::utility::time_f() - start) * 1000;
		embed.add_field("DB ping", std::to_string(db_ping), true);
	}

	event.reply(dpp::message().add_embed(embed));
}

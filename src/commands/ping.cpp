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
#include <beholder/commands/ping.h>
#include <beholder/database.h>
#include <fmt/format.h>

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
		.set_thumbnail(bot->me.get_avatar_url())
		.set_description(":ping_pong: Ping information:");

	/* Get the rest ping from discord. */
	double discord_api_ping = bot->rest_ping * 1000;

	embed.add_field("REST Ping", fmt::format("{:.02f}ms", discord_api_ping), true);

	/* Calculate the DB time */
	double start = dpp::utility::time_f();
	db::resultset q = db::query("SHOW TABLES");

	double db_ping = (dpp::utility::time_f() - start) * 1000;
	embed.add_field("Database Ping", fmt::format("{:.02f}ms", db_ping), true);

	event.reply(dpp::message().add_embed(embed));
}

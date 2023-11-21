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
#include <dpp/dpp.h>
#include <dpp/json.h>
#include <fmt/format.h>
#include <beholder/beholder.h>
#include <beholder/listeners.h>
#include <beholder/database.h>
#include <beholder/logger.h>
#include <beholder/config.h>
#include <beholder/sentry.h>

int main(int argc, char const *argv[])
{
	std::srand(time(NULL));
	config::init("../config.json");
	logger::init(config::get("log"));

	dpp::cluster bot(
		config::get("token"),
		dpp::i_guild_messages | dpp::i_message_content | dpp::i_guild_members,
		1, 0, 1, true, dpp::cache_policy::cpol_none
	);

	sentry::init(bot);

	bot.on_log(&logger::log);
	bot.on_slashcommand(&listeners::on_slashcommand);
	bot.on_message_create(&listeners::on_message_create);
	bot.on_message_update(&listeners::on_message_update);
	bot.on_button_click(&listeners::on_button_click);
	bot.on_ready(&listeners::on_ready);

	db::init(bot);

	/* Start bot */
	bot.set_websocket_protocol(dpp::ws_etf);
	bot.start(dpp::st_wait);

	sentry::close();
}


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
#include <beholder/commands/info.h>
#include <beholder/database.h>
#include <beholder/listeners.h>

dpp::slashcommand info_command::register_command(dpp::cluster& bot)
{
	return dpp::slashcommand("info", "Show bot information", bot.me.id);
}

int64_t proc_self_value(const std::string& find_token) {
	int64_t ret = 0;
	std::ifstream self_status("/proc/self/status");
	while (self_status) {
		std::string token;
		self_status >> token;
		if (token == find_token) {
			self_status >> ret;
			break;
		}
	}
	self_status.close();
	return ret;
}

int64_t rss() {
	return proc_self_value("VmRSS:") * 1024;
}

bool is_gdb() {
	return proc_self_value("TracerPid:") != 0;
}

dpp::task<void> info_command::route(const dpp::slashcommand_t &event)
{
	dpp::cluster* bot = event.owner;
	auto v = co_await bot->co_current_application_get();
	dpp::application app;
	uint64_t guild_count = 0;
	if (!v.is_error()) {
		app = std::get<dpp::application>(v.value);
		guild_count = app.approximate_guild_count;
	}
	std::string pattern_count{"0"}, log_channel, scanned_today{"0"}, blocked_today{"0"};
	bool has_premium = false;
	db::resultset config = co_await db::co_query("SELECT gc.*, COUNT(gp.pattern) AS patterns FROM guild_config gc LEFT JOIN guild_patterns gp ON gp.guild_id = gc.guild_id WHERE gc.guild_id = ? GROUP BY gc.guild_id", { event.command.guild_id });
	if (!config.empty()) {
		pattern_count = config[0].at("patterns");
		has_premium = !(config[0].at("premium_subscription").empty());
		log_channel = config[0].at("log_channel");
	}
	db::resultset stats = co_await db::co_query("SELECT images_scanned, (images_blocked + images_ocr + images_nsfw) AS images_deleted FROM guild_statistics WHERE guild_id = ? AND stat_date = CURDATE()", { event.command.guild_id });
	if (!stats.empty()) {
		scanned_today = stats[0].at("images_scanned");
		blocked_today = stats[0].at("images_deleted");
	}

	dpp::embed embed = dpp::embed()
		.set_url("https://beholder.cc/")
		.set_title("Beholder Information")
		.set_footer(dpp::embed_footer{
			.text = "Requested by " + event.command.usr.format_username(),
			.icon_url = bot->me.get_avatar_url(),
			.proxy_url = "",
		})
		.set_colour(0x7aff7a)
		.set_thumbnail("https://beholder.cc/img/beholder_animated.gif")
		.set_description(has_premium ? ":star: This server has Beholder Premium! :star:" : "")
		.add_field("Text Patterns", pattern_count, true)
		.add_field("Images Scanned Today", scanned_today, true)
		.add_field("Images Blocked Today", blocked_today, true)
		.add_field("Bot Uptime", bot->uptime().to_string(), true)
		.add_field("Memory Usage", std::to_string(rss() / 1024 / 1024) + "M", true)
		.add_field("Total Servers", std::to_string(guild_count), true)
		.add_field("Log Channel", log_channel.length() ? "<#" + log_channel + ">" : "(not set)", true)
		.add_field("Scans In Progress", std::to_string(tessd_process_count()), true)
		.add_field("Debugging", is_gdb() ? ":white_check_mark: Yes" : "<:wc_rs:667695516737470494> No", true)
		.add_field("Guild Members Intent", ":white_check_mark: Yes", true)
		.add_field("Message Content Intent", ":white_check_mark: Yes", true)
		.add_field("Shard", std::to_string(event.shard) + "/" + std::to_string(bot->get_shards().size()), true);

	embed.add_field("Library Version", "<:DPP1:847152435399360583><:DPP2:847152435343523881> [" + std::string(DPP_VERSION_TEXT) + "](https://dpp.dev/)", false);

	event.co_reply(dpp::message().add_embed(embed));
	co_return;
}
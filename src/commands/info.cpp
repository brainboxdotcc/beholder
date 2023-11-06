#include <beholder/beholder.h>
#include <beholder/database.h>
#include <beholder/commands/info.h>
#include <beholder/database.h>
#include <beholder/sentry.h>

dpp::slashcommand info_command::register_command(dpp::cluster& bot)
{
	return dpp::slashcommand("info", "Show bot information", bot.me.id);
}

int64_t rss() {
	int64_t ram = 0;
	std::ifstream self_status("/proc/self/status");
	while (self_status) {
		std::string token;
		self_status >> token;
		if (token == "VmRSS:") {
			self_status >> ram;
			break;
		}
	}
	self_status.close();
	return ram * 1024;
}

void info_command::route(const dpp::slashcommand_t &event)
{
	dpp::cluster* bot = event.from->creator;
	bot->current_application_get([bot, event](const dpp::confirmation_callback_t& v) {
		dpp::application app;
		uint64_t guild_count = 0;
		if (!v.is_error()) {
			app = std::get<dpp::application>(v.value);
			guild_count = app.approximate_guild_count;
		}
		std::string pattern_count{"0"}, prem_count{"0"}, log_channel;
		bool has_premium = false;
		db::resultset config = db::query("SELECT *, (SELECT COUNT(pattern) FROM guild_patterns WHERE guild_id = ?) AS patterns, (SELECT COUNT(pattern) FROM premium_filters WHERE guild_id = ?) AS premium_patterns FROM guild_config WHERE guild_id = ?", { event.command.guild_id.str(), event.command.guild_id.str(), event.command.guild_id.str() });
		if (!config.empty()) {
			pattern_count = config[0].at("patterns");
			prem_count = config[0].at("premium_patterns");
			has_premium = !(config[0].at("premium_subscription").empty());
			log_channel = config[0].at("log_channel");
		}

		dpp::embed embed = dpp::embed()
			.set_url("https://beholder.cc/")
			.set_title("Beholder Information")
			.set_colour(0x7aff7a)
			.set_thumbnail(app.get_icon_url())
			.set_description(has_premium ? ":star: This server has Beholder Premium! :star:" : "")
			.add_field("Text Patterns", pattern_count, true)
			.add_field("Image Rules", prem_count, true)
			.add_field("Bot Uptime", bot->uptime().to_string(), true)
			.add_field("Memory Usage", std::to_string(rss() / 1024 / 1024) + "M", true)
			.add_field("Total Servers", std::to_string(guild_count), true)
			.add_field("Concurrency", std::to_string(concurrent_images), true)
			.add_field("Sentry Version", sentry::version(), true);

		if (!log_channel.empty()) {
			embed.add_field("Log Channel", "<#" + log_channel + ">", true);
		}
		embed.add_field("Library Version", "<:DPP1:847152435399360583><:DPP2:847152435343523881> [" + std::string(DPP_VERSION_TEXT) + "](https://dpp.dev/)", false);

		event.reply(dpp::message().add_embed(embed));
	});
}

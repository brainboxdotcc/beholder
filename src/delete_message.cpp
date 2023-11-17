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
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <fmt/format.h>

void delete_message_and_warn(const std::string& image, dpp::cluster& bot, const dpp::message_create_t ev, const dpp::attachment attach, const std::string text, bool premium, double trigger, double threshold)
{
	bot.message_delete(ev.msg.id, ev.msg.channel_id, [&bot, ev, attach, text, premium, image, trigger, threshold](const auto& cc) {

		bool delete_failed = cc.is_error();

		db::resultset logchannel;
		if (premium) {
			logchannel = db::query("SELECT embeds_disabled, log_channel, premium_title as embed_title, premium_body as embed_body FROM guild_config WHERE guild_id = ?", { ev.msg.guild_id });
		} else {
			logchannel = db::query("SELECT embeds_disabled, log_channel, embed_title, embed_body FROM guild_config WHERE guild_id = ?", { ev.msg.guild_id });
		}

		if (logchannel.size()) {

			if (logchannel[0].at("embeds_disabled") == "0") {
				std::string message_body = logchannel[0].at("embed_body");
				std::string message_title = logchannel[0].at("embed_title");

				if (message_body.empty()) {
					message_body = "Please set a message using " + std::string(premium ? "`/premium message`" : "`/message content`");
				}
				if (message_title.empty()) {
					message_title = "Please set a title using " + std::string(premium ? "`/premium message`" : "`/message content`");
				}
				message_body = replace_string(message_body, "@user", ev.msg.author.get_mention());

				bot.message_create(
					dpp::message(ev.msg.channel_id, "")
					.add_embed(
						dpp::embed()
						.set_description(message_body)
						.set_title(message_title)
						.set_color(colours::bad)
						.set_url("https://beholder.cc/")
						.set_thumbnail(bot.me.get_avatar_url())
						.set_footer("Powered by Beholder", bot.me.get_avatar_url())
					)
				);
			}

			if (logchannel[0].at("log_channel").length()) {

				if (delete_failed) { 
					bot.message_create(dpp::message(dpp::snowflake(logchannel[0].at("log_channel")), "Failed to delete message: " + dpp::utility::message_url(ev.msg.guild_id, ev.msg.channel_id, ev.msg.id) + " - Please check bot permissions."));
				}

				bot.message_create(
					dpp::message(dpp::snowflake(logchannel[0].at("log_channel")), "")
					.add_embed(
						dpp::embed()
						.set_description(
							"Attachment: `" + attach.filename + "`\nSent by: `" +
							ev.msg.author.format_username() + "` (" +
							ev.msg.author.get_mention() + ")" +
							"\nIn Channel: <#" + ev.msg.channel_id.str() + ">" +
							"\nMatched pattern: `" + text + "`"
							+ (premium && trigger ? fmt::format("\nProbability: `{:.1f}%`, Threshold: `{:.1f}%`", trigger * 100.0, threshold * 100) : "")
							+ "\n[Image link](" + attach.url +")"
						)
						.set_title("Bad Image Deleted")
						.set_color(colours::good)
						.set_image("attachment://" + attach.filename)
						.set_url("https://beholder.cc/")
						.set_thumbnail(bot.me.get_avatar_url())
						.set_footer("Powered by Beholder - Message ID " + std::to_string(ev.msg.id), bot.me.get_avatar_url())
					).add_file(attach.filename, image)
				);
			}
		}
	});
}

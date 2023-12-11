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
#include <dpp/unicode_emoji.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <fmt/format.h>

void delete_message_and_warn(const std::string& hash, const std::string& image, dpp::cluster& bot, const dpp::message_create_t ev, const dpp::attachment attach, const std::string text, bool premium, double trigger, double threshold)
{
	bot.message_delete(ev.msg.id, ev.msg.channel_id, [hash, &bot, ev, attach, text, premium, image, trigger, threshold](const auto& cc) {

		bool delete_failed = cc.is_error();

		db::resultset logchannel;
		if (premium) {
			logchannel = db::query("SELECT embeds_disabled, log_channel, premium_title as embed_title, premium_body as embed_body FROM guild_config WHERE guild_id = ?", { ev.msg.guild_id });
		} else {
			logchannel = db::query("SELECT embeds_disabled, log_channel, embed_title, embed_body FROM guild_config WHERE guild_id = ?", { ev.msg.guild_id });
		}

		if (logchannel.size() == 0 || logchannel[0].at("embeds_disabled") == "0") {
			std::string message_body = logchannel.size() == 0 ? "" : logchannel[0].at("embed_body");
			std::string message_title = logchannel.size() == 0 ? "" : logchannel[0].at("embed_title");

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

		if (logchannel.size()) {

			if (logchannel[0].at("log_channel").length()) {

				if (delete_failed) { 
					bot.message_create(dpp::message(dpp::snowflake(logchannel[0].at("log_channel")), "Failed to delete message: " + dpp::utility::message_url(ev.msg.guild_id, ev.msg.channel_id, ev.msg.id) + " - Please check bot permissions."));
				}

				dpp::message delete_msg;
				delete_msg.set_channel_id(logchannel[0].at("log_channel")).add_embed(
					dpp::embed()
					.set_description("**Attachment:** `" + attach.filename + "`\n🔗 [Image link](" + attach.url +")")
					.add_field("Sent By", "`" + ev.msg.author.format_username() + "`", true)
					.add_field("Mention", ev.msg.author.get_mention(), true)
					.add_field("In Channel", "<#" + ev.msg.channel_id.str() + ">", true)
					.set_title("🚫 Image Deleted!")
					.set_color(colours::good)
					.set_image("attachment://" + attach.filename)
					.set_url("https://beholder.cc/")
					.set_footer("Powered by Beholder - Message ID " + std::to_string(ev.msg.id), bot.me.get_avatar_url())
				).add_file(attach.filename, image);
				if (premium && trigger) {
					delete_msg.embeds[0].add_field("AI Probability", fmt::format("`{:.1f}%`", trigger * 100.0), true);
					delete_msg.embeds[0].add_field("AI Trigger Setting", fmt::format("`{:.1f}%`", threshold * 100), true);
				}
				delete_msg.embeds[0].add_field("Matched Pattern", "```\n" + text + "\n```", false);
				delete_msg.add_component(
					dpp::component()
					.add_component(dpp::component()
						.set_label("Block")
						.set_type(dpp::cot_button)
						.set_emoji(dpp::unicode_emoji::no_entry)
						.set_style(dpp::cos_danger)
						.set_id("BL;*;" + hash)
					)
					.add_component(dpp::component()
						.set_label("Unblock")
						.set_type(dpp::cot_button)
						.set_emoji(dpp::unicode_emoji::white_check_mark)
						.set_style(dpp::cos_success)
						.set_id("UB;*;" + hash)
					)
				);					
				if (premium && trigger >= 0.4) {
					auto rs = db::query("SELECT model FROM premium_filter_model WHERE description = ?", { text });
					if (rs.size()) {
						delete_msg.components[0]
						.add_component(dpp::component()
							.set_label("False Positive")
							.set_type(dpp::cot_button)
							.set_emoji(dpp::unicode_emoji::thumbsdown)
							.set_style(dpp::cos_danger)
							.set_id("DN;" + rs[0].at("model") + ";" + hash)
						)
						.add_component(dpp::component()
							.set_label("Good Match")
							.set_type(dpp::cot_button)
							.set_emoji(dpp::unicode_emoji::thumbsup)
							.set_style(dpp::cos_success)
							.set_id("UP;" + rs[0].at("model") + ";" + hash)
						);
					}
				}
				bot.message_create(delete_msg);
			}
		}
	});
}

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
#include <dpp/dpp.h>
#include <dpp/unicode_emoji.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <fmt/core.h>
#include <deque>
#include <mutex>
#include <unordered_set>

namespace {

	constexpr size_t deleted_message_window_size = 4096;

	std::mutex deleted_message_mutex;
	std::deque<dpp::snowflake> deleted_message_order;
	std::unordered_set<dpp::snowflake> deleted_message_ids;

	bool mark_message_delete_attempted(dpp::snowflake message_id)
	{
		std::lock_guard<std::mutex> lock(deleted_message_mutex);

		if (deleted_message_ids.contains(message_id)) {
			return false;
		}

		deleted_message_ids.insert(message_id);
		deleted_message_order.emplace_back(message_id);

		while (deleted_message_order.size() > deleted_message_window_size) {
			deleted_message_ids.erase(deleted_message_order.front());
			deleted_message_order.pop_front();
		}

		return true;
	}

}

dpp::task<bool> delete_message_and_warn(std::string hash, std::string image, dpp::cluster& bot, const dpp::message_create_t ev, const dpp::attachment attach, const std::string text, double trigger, double threshold)
{
	bot.log(dpp::ll_info, "in delete_message_and_warn; cid=" + ev.msg.channel_id.str() + " mid=" + ev.msg.id.str() + " hash=" + hash);

	const bool should_delete = mark_message_delete_attempted(ev.msg.id);
	if (!should_delete) {
		bot.log(dpp::ll_info, "message already deleted or delete already attempted; mid=" + ev.msg.id.str() + " hash=" + hash);
		co_return false;
	}

	/**
	 * Note: Once we have confirmation the delete succeeded here, we do not have to await the warning and modlog message. They can be
	 * fired asynchronously without a co_await as this is faster when not sent serially.
	 */

	auto cc = co_await bot.co_message_delete(ev.msg.id, ev.msg.channel_id);

	bool delete_failed = cc.is_error();

	db::resultset logchannel = co_await db::co_query("SELECT embeds_disabled, log_channel, embed_title, embed_body FROM guild_config WHERE guild_id = ?", {ev.msg.guild_id});

	if (logchannel.size() == 0 || logchannel[0].at("embeds_disabled") == "0") {
		std::string message_body = logchannel.size() == 0 ? "" : logchannel[0].at("embed_body");
		std::string message_title = logchannel.size() == 0 ? "" : logchannel[0].at("embed_title");

		if (message_body.empty()) {
			message_body = "This message contained disallowed image content!\n\nModerators can configure this message using `/message content`";
		}
		if (message_title.empty()) {
			message_title = "Please set a title using `/message content`";
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
				bot.message_create(
					dpp::message(
						dpp::snowflake(logchannel[0].at("log_channel")),
						"Failed to delete message: " + dpp::utility::message_url(ev.msg.guild_id, ev.msg.channel_id, ev.msg.id) + " - Please check bot permissions."
						)
					);
			}

			dpp::message delete_msg;
			delete_msg.set_channel_id(logchannel[0].at("log_channel")).add_embed(
				dpp::embed()
					.set_description("**Attachment:** `" + attach.filename + "`\n🔗 [Image link](" + attach.url + ")")
					.add_field("Sent By", "`" + ev.msg.author.format_username() + "`", true)
					.add_field("Mention", ev.msg.author.get_mention(), true)
					.add_field("In Channel", "<#" + ev.msg.channel_id.str() + ">", true)
					.set_title("🚫 Image Deleted!")
					.set_color(colours::good)
					.set_url("https://beholder.cc/")
					.set_footer("Powered by Beholder - Message ID " + std::to_string(ev.msg.id), bot.me.get_avatar_url())
			);
			delete_msg.embeds[0].add_field("Matched Pattern", "```\n" + text + "\n```", false);
			delete_msg.add_component(
				dpp::component()
					.add_component(dpp::component()
					       .set_label("Block").set_type(dpp::cot_button)
					       .set_emoji(dpp::unicode_emoji::no_entry)
					       .set_style(dpp::cos_danger).set_id("BL;*;" + hash)
					)
					.add_component(dpp::component()
					       .set_label("Unblock").set_type(dpp::cot_button)
					       .set_emoji(dpp::unicode_emoji::white_check_mark)
					       .set_style(dpp::cos_success).set_id("UB;*;" + hash)
					)
					.add_component(dpp::component()
					       .set_label("Kick User").set_type(dpp::cot_button)
					       .set_emoji(dpp::unicode_emoji::foot)
					       .set_style(dpp::cos_primary).set_id("KI;*;" + ev.msg.author.id.str())
					)
					.add_component(dpp::component()
					       .set_label("Timeout User").set_type(dpp::cot_button)
					       .set_emoji(dpp::unicode_emoji::clock)
					       .set_style(dpp::cos_primary).set_id("TI;*;" + ev.msg.author.id.str())
					)
					.add_component(dpp::component()
					       .set_label("Ban User").set_type(dpp::cot_button)
					       .set_emoji(dpp::unicode_emoji::cop)
					       .set_style(dpp::cos_primary).set_id("BA;*;" + ev.msg.author.id.str())
					)
			);
			bot.message_create(delete_msg);
		}
	}
	co_return should_delete;
}

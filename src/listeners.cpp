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
#include <set>
#include <fmt/format.h>
#include <CxxUrl/url.hpp>
#include <beholder/listeners.h>
#include <beholder/database.h>
#include <beholder/beholder.h>
#include <beholder/command.h>
#include <beholder/sentry.h>
#include <beholder/premium_api.h>

#include <beholder/commands/logchannel.h>
#include <beholder/commands/roles.h>
#include <beholder/commands/message.h>
#include <beholder/commands/patterns.h>
#include <beholder/commands/premium.h>
#include <beholder/commands/info.h>
#include <beholder/commands/ping.h>
#include <beholder/commands/ignoredchannels.h>
#include <beholder/commands/addblock.h>
#include <beholder/commands/showtags.h>
#include <beholder/commands/scan.h>

#include <beholder/botlist.h>
#include <beholder/botlists/topgg.h>
#include <beholder/botlists/discordbotlist.h>
#include <beholder/botlists/infinitybots.h>

namespace listeners {

	/**
	 * @brief Welcome a new guild to the bot with some advice on getting started
	 * 
	 * @param bot 
	 * @param guild_id 
	 * @param channel_id 
	 */
	void send_welcome(dpp::cluster& bot, dpp::snowflake guild_id, dpp::snowflake channel_id) {
		bot.message_create(
			dpp::message(channel_id, "")
			.add_embed(
				dpp::embed()
				.set_description("Thank you for inviting the **Beholder** Bot! You can configure this \
bot either via application commands, or [through the Beholder Dashboard](https://beholder.cc/dashboard) \
\n\n\
To get started, use the `/logchannel` command to set the channel where the bot will send \
alerts of blocked images, and use `/patterns` to set up words that are banned within images. \
You can also use the `/ignorechannels` and `/roles` command to set up channels or roles that \
Beholder will not scan images for. \
\n\n\
**Beholder's default settings will block various NSFW image types automatically,** so long as a log channel is configured. \
\n\n\
For more in depth configuration please see the [bot dashboard](https://beholder.cc/dashboard) - \
You can get help with the bot on our [support server](https://discord.com/invite/Ac2duhZAr5). \
\n\n\
For advanced NSFW filtering, with 25 different categories, please consider subscribing to \
[Beholder Premium](https://beholder.cc/premium) - Prices start at only £3 a month, less than a good cup of coffee! :coffee:\
")
				.set_title("Thank You for Inviting Beholder!")
				.set_color(colours::good)
				.set_url("https://beholder.cc/")
				.set_thumbnail(bot.me.get_avatar_url())
				.set_footer("Powered by Beholder", bot.me.get_avatar_url())
			)
		);
		/* Probably successfully welcomed */
		db::query("UPDATE guild_cache SET welcome_sent = 1 WHERE id = ?", {guild_id});
	}

	/**
	 * @brief Check every 30 seconds for new guilds and welcome them
	 * 
	 * @param bot cluster ref
	 */
	void welcome_new_guilds(dpp::cluster& bot) {
		auto result = db::query("SELECT id FROM guild_cache WHERE welcome_sent = 0");
		for (const auto& row : result) {
			/* Determine the correct channel to send to */
			dpp::snowflake guild_id = row.at("id");
			bot.log(dpp::ll_info, "New guild: " + guild_id.str());
			bot.guild_get(guild_id, [&bot, guild_id](const auto& cc) {
				if (cc.is_error()) {
					/* Couldn't fetch the guild - kicked within 30 secs of inviting, bummer. */
					db::query("UPDATE guild_cache SET welcome_sent = 1 WHERE id = ?", {guild_id});
					return;
				}
				dpp::guild guild = std::get<dpp::guild>(cc.value);
				/* First try to send the message to system channel or safety alerts channel if defined */
				if (!guild.system_channel_id.empty()) {
					send_welcome(bot, guild.id, guild.system_channel_id);
					return;
				}
				if (!guild.safety_alerts_channel_id.empty()) {
					send_welcome(bot, guild.id, guild.safety_alerts_channel_id);
					return;
				}
				/* As a last resort if they dont have one of those channels set up, find a named
				 * text channel that looks like its the main general/chat channel
				 */
				bot.channels_get(guild_id, [&bot, guild_id, guild](const auto& cc) {
					if (cc.is_error()) {
						/* Couldn't fetch the channels - kicked within 30 secs of inviting, bummer. */
						db::query("UPDATE guild_cache SET welcome_sent = 1 WHERE id = ?", {guild_id});
						return;
					}
					dpp::channel_map channels = std::get<dpp::channel_map>(cc.value);
					dpp::snowflake selected_channel_id, first_text_channel_id;
					for (const auto& c : channels) {
						const dpp::channel& channel = c.second;
						std::string lowername = dpp::lowercase(channel.name);
						if ((lowername == "general" || lowername == "chat" || lowername == "moderators") && channel.is_text_channel()) {
							selected_channel_id = channel.id;
							break;
						} else if (channel.is_text_channel()) {
							first_text_channel_id = channel.id;
						}
					}
					if (selected_channel_id.empty() && !first_text_channel_id.empty()) {
						selected_channel_id = first_text_channel_id;
					}
					if (!selected_channel_id.empty()) {
						send_welcome(bot, guild_id, selected_channel_id);
					} else {
						/* What sort of server has NO text channels and invites an image moderation bot??? */
						db::query("UPDATE guild_cache SET welcome_sent = 1 WHERE id = ?", {guild_id});
					}
				});
			});
		}
	}

	void on_ready(const dpp::ready_t &event) {
		dpp::cluster& bot = *event.from->creator;
		if (dpp::run_once<struct register_bot_commands>()) {
			bot.global_bulk_command_create({
				register_command<info_command>(bot),
				register_command<premium_command>(bot),
				register_command<patterns_command>(bot),
				register_command<roles_command>(bot),
				register_command<message_command>(bot),
				register_command<logchannel_command>(bot),
				register_command<ping_command>(bot),
				register_command<ignoredchannels_command>(bot),
				register_command<addblock_command>(bot),
				register_command<showtags_command>(bot),
				register_command<scan_command>(bot),
			});

			auto set_presence = [&bot]() {
				bot.current_application_get([&bot](const dpp::confirmation_callback_t& v) {
					if (!v.is_error()) {
						dpp::application app = std::get<dpp::application>(v.value);
						bot.set_presence(dpp::presence(dpp::ps_online, dpp::at_watching, fmt::format("images on {} servers", app.approximate_guild_count)));
					}
				});
			};

			bot.start_timer([&bot, set_presence](dpp::timer t) {
				set_presence();
			}, 240);
			bot.start_timer([&bot](dpp::timer t) {
				post_botlists(bot);
			}, 60 * 15);
			bot.start_timer([&bot](dpp::timer t) {
				welcome_new_guilds(bot);
			}, 30);

			set_presence();
			welcome_new_guilds(bot);

			register_botlist<topgg>();
			register_botlist<discordbotlist>();
			register_botlist<infinitybots>();

			post_botlists(bot);
		}
	}

	void on_guild_create(const dpp::guild_create_t &event) {
		if (event.created->is_unavailable()) {
			return;
		}
		db::query("INSERT INTO guild_cache (id, owner_id, name, user_count) VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE owner_id = ?, name = ?, user_count = ?", { event.created->id, event.created->owner_id, event.created->name, event.created->member_count, event.created->owner_id, event.created->name, event.created->member_count });
	}

	void on_guild_delete(const dpp::guild_delete_t &event) {
		db::query("DELETE FROM guild_cache WHERE id = ?", { event.deleted.id });
		db::query("DELETE FROM guild_config WHERE guild_id = ?", { event.deleted.id });
		db::query("DELETE FROM guild_statistics WHERE guild_id = ?", { event.deleted.id });
		event.from->creator->log(dpp::ll_info, "Removed from guild: " + event.deleted.id.str());
	}

	void on_button_click(const dpp::button_click_t &event) {
		bool ours = false;
		bool good = false;
		dpp::permission p = event.command.get_resolved_permission(event.command.usr.id);
		if (!p.has(dpp::p_manage_guild)) {
			event.reply(dpp::message(event.command.channel_id, "You require the manage server permission to add images to the block list.").set_flags(dpp::m_ephemeral));
			return;
		}
		dpp::cluster& bot = *(event.from->creator);
		std::vector<std::string> parts = dpp::utility::tokenize(event.custom_id, ";");
		auto logchannel = db::query("SELECT embeds_disabled, log_channel, embed_title, embed_body FROM guild_config WHERE guild_id = ?", { event.command.guild_id });
		if (logchannel.size()) {
			dpp::snowflake channel_id(logchannel[0].at("log_channel"));
			dpp::snowflake message_id = event.command.message_id;
			if (parts[0] == "DN") {
				/* Report false positive */
				ours = true;
				good = false;
				event.reply(":+1: Thank you for reporting a possible **false positive**, " + event.command.usr.get_mention() + ". This feedback will be used to train the AI.");
			} else if (parts[0] == "UP") {
				/* Report good match */
				ours = true;
				good = true;
				event.reply(":+1: Thank you for confirming this was a **good result**, " + event.command.usr.get_mention() + ". This feedback will be used to train the AI.");
			} else if (parts[0] == "BL") {
				/* Report good match */
				std::string hash = parts[2];				
				db::query("INSERT INTO block_list_items (guild_id, hash) VALUES(?,?) ON DUPLICATE KEY UPDATE hash = ?", { event.command.guild_id, hash, hash });
				event.reply(":no_entry: This image has been **added to the block list** by " + event.command.usr.get_mention() + ". It will be **instantly deleted** without performing any further checks.");
			} else if (parts[0] == "UB") {
				/* Report good match */
				std::string hash = parts[2];
				db::query("DELETE FROM block_list_items WHERE guild_id = ? AND hash = ?", { event.command.guild_id, hash });
				event.reply(":white_check_mark: This image has been **removed from the block list** by " + event.command.usr.get_mention() + ". It will now be **checked normally**.");
			}

			if (ours) {
				bot.message_get(message_id, channel_id, [event, good, message_id, channel_id, parts](const auto& cc) {
					if (cc.is_error()) {
						return;
					}
					dpp::message m = std::get<dpp::message>(cc.value);
					if (m.embeds.size() && m.embeds[0].image.has_value()) {
						premium_api::report(*(event.from->creator), good, message_id, channel_id, m.embeds[0].image->url, parts[1]);
					}
				});
			}
		}
	}

	void on_slashcommand(const dpp::slashcommand_t &event) {
		event.from->creator->log(
			dpp::ll_info,
			fmt::format(
				"COMMAND: {} by {} ({} Guild: {})",
				event.command.get_command_name(),
				event.command.usr.format_username(),
				event.command.usr.id,
				event.command.guild_id
			)
		);
		route_command(event);
	}

	void sarcastic_ping(const dpp::message_create_t &ev) {
		std::vector<std::string> replies{
			"@user stop poking me!",
			"@user that tickles!",
			"Yes, @user?",
			"What, @user?",
			"@user, yes, im here, hello!",
			"I'm a bot, @user, I dont do idle conversation",
			"@user, can't you see i have a job to do?",
			"@user HAIIIII <a:kekeke:959959620430995557>",
			"I am a bot, and i'm watching your images.",
			"@user, do you have a moment to hear about Brain, our lord and saviour?",
			"@user you're sus.",
			"@user perhaps you want `/info`.",
			"@user I was asleep. Thanks for waking me.",
			"DND, currently yeeting your message in another channel",
			"@user, i was going to leave you on read but i didnt want to hurt your feelings",
			"Your command?",
			"I hear you.",
			"Your will?",
			"@user I don't have time for games!",
			"@user do not try my patience!",
			"How may I help?",
			"You must construct additional pylons.",
			"I am sworn to ~~carry your burdens~~ watch your chat",
			"Developer todo: *Insert witty bot response before release*",
			"https://media.tenor.com/I0R6wPzYDiMAAAAd/buzz-lightyear-that-seems-to-be-no-sign-of-intelligent-life-anywhere.gif",
			"https://media.tenor.com/Xp-gkoRvnIEAAAAd/dumb-wizard-of-ozz.gif",
			"hello @user?",
			"For more information on what this bot is, visit <https://beholder.cc>",
		};
		std::vector<std::string>::iterator rand_iter = replies.begin();
		std::advance(rand_iter, std::rand() % replies.size());
		std::string response = replace_string(*rand_iter, "@user", ev.msg.author.get_mention());
		ev.from->creator->message_create(
			dpp::message(ev.msg.channel_id, response)
				.set_allowed_mentions(true, false, false, true, {}, {})
				.set_reference(ev.msg.id, ev.msg.guild_id, ev.msg.channel_id, false)
		);
	}

	void on_message_update(const dpp::message_update_t &event) {
		/* Message update is mapped to message creation.
		 * The effect is the same, the message is simply re-scanned.
		 */
		dpp::message_create_t c(event.from, event.raw_event);
		c.msg = event.msg;
		on_message_create(c);
	}

	void on_message_create(const dpp::message_create_t &event) {
		auto guild_member = event.msg.member;

		/* If the author is a bot or webhook, stop the event (no checking). */
		if (event.msg.author.is_bot() || event.msg.author.id.empty()) {
			return;
		}

		/* Check if we are mentioned in the message, if so send a sarcastic reply */
		for (const auto& ping : event.msg.mentions) {
			if (ping.first.id == event.from->creator->me.id) {
				sarcastic_ping(event);
				/* Don't return, just break, because people might still try
				 * to put dodgy images in the message too
				 */
				break;
			}
		}

		/* Check for channels that are ignored */
		db::resultset bypass_channel = db::query("SELECT * FROM guild_ignored_channels WHERE guild_id = ? AND channel_id = ?", { event.msg.guild_id, event.msg.channel_id });
		if (bypass_channel.size() > 0) {
			return;
		}

		/* Loop through all bypass roles in database */
		db::resultset bypass_roles = db::query("SELECT * FROM guild_bypass_roles WHERE guild_id = ?", { event.msg.guild_id });
		for (const db::row& role : bypass_roles) {
			const auto& roles = guild_member.get_roles();
			if (std::find(roles.begin(), roles.end(), dpp::snowflake(role.at("role_id"))) != roles.end()) {
				/* Stop the event if user is in a bypass role */
				return;
			}
		}

		/* Check each attachment in the message, if any */
		if (event.msg.attachments.size() > 0) {
			for (const dpp::attachment& attach : event.msg.attachments) {
				download_image(attach, *event.from->creator, event);
			}
		}

		/* Split the message by spaces and newlines */
		std::vector<std::string> parts = dpp::utility::tokenize(replace_string(event.msg.content, "\n", " "), " ");

		/* Check for images in the embeds of the message, if any. Found urls and
		 * scannable content are appended into the parts vector for scanning as text content.
		 *
		 * You might be asking "we do we look in embeds, if we ignore messages from other bots, and embeds can
		 * only be legally sent by bots?" There are two reasons for this: Firstly, when a user sends a link
		 * that can be automatically embedded by Discord, such as a YouTube link, an embed is automatically generated
		 * and associated with the message. The second reason is detecting misbehaviour by selfbots.
		 */
		if (event.msg.embeds.size() > 0) {
			for (const dpp::embed& embed : event.msg.embeds) {
				if (!embed.url.empty()) {
					parts.emplace_back(embed.url);
				}
				if (embed.thumbnail.has_value() && !embed.thumbnail->url.empty()) {
					parts.emplace_back(embed.thumbnail->url);
				}
				if (embed.footer.has_value() && !embed.footer->icon_url.empty()) {
					parts.emplace_back(embed.footer->icon_url);
				}
				if (embed.image.has_value() && !embed.image->url.empty()) {
					parts.emplace_back(embed.image->url);
				}
				if (embed.video.has_value() && !embed.video->url.empty()) {
					parts.emplace_back(embed.video->url);
				}
				if (embed.author.has_value()) {
					if (!embed.author->icon_url.empty()) {
						parts.emplace_back(embed.author->icon_url);
					}
					if (!embed.author->url.empty()) {
						parts.emplace_back(embed.author->url);
					}
				}
				auto spaced = dpp::utility::tokenize(replace_string(embed.description, "\n", " "), " ");
				if (!spaced.empty()) {
					parts.insert(parts.end(), spaced.begin(), spaced.end());
				}
				for (const dpp::embed_field& field : embed.fields) {
					auto spaced = dpp::utility::tokenize(replace_string(field.value, "\n", " "), " ");
					if (!spaced.empty()) {
						parts.insert(parts.end(), spaced.begin(), spaced.end());
					}
				}
			}
		}

		/* Extract sticker urls, if any stickers are in the image */
		if (event.msg.stickers.size() > 0) {
			for (const dpp::sticker& sticker : event.msg.stickers) {
				if (!sticker.id.empty()) {
					parts.emplace_back(sticker.get_url());
				}
			}
		}

		/* Used to deduplicate the urls */
		std::set<std::string> checked;
		/* Check each word in the message looking for URLs */
		for (std::string& possibly_url : parts) {
			size_t size = possibly_url.length();
			std::string original_url = possibly_url;
			possibly_url = dpp::lowercase(possibly_url);
			/* Check for markdown cloaked urls: [descr potentially with spaces](url) */
			auto cloaked_url_pos = possibly_url.find("](http");
			if (cloaked_url_pos != std::string::npos && possibly_url.length() - cloaked_url_pos - 3 > 7) {
				possibly_url = possibly_url.substr(cloaked_url_pos + 2, possibly_url.length() - cloaked_url_pos - 3);
				original_url = original_url.substr(cloaked_url_pos + 2, original_url.length() - cloaked_url_pos - 3);
			}
			if ((size >= 9 && possibly_url.substr(0, 8) == "https://") ||
			(size >= 8 && possibly_url.substr(0, 7) == "http://")) {
				dpp::attachment attach((dpp::message*)&event.msg);
				attach.url = original_url;
				try {
					Url u(original_url);
					attach.filename = fs::path(u.path()).filename();
				}
				catch (const std::exception&) {
					attach.filename = fs::path(original_url).filename();
				}
				if (checked.find(original_url) == checked.end()) {
					download_image(attach, *event.from->creator, event);
					checked.emplace(original_url);
				}
			}
		}
	}
}

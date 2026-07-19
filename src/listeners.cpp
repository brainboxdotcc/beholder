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
#include <set>
#include <fmt/format.h>
#include <CxxUrl/url.hpp>
#include <beholder/listeners.h>
#include <beholder/database.h>
#include <beholder/beholder.h>
#include <beholder/command.h>
#include <beholder/sarcasm.h>

#include <beholder/commands/logchannel.h>
#include <beholder/commands/roles.h>
#include <beholder/commands/message.h>
#include <beholder/commands/patterns.h>
#include <beholder/commands/info.h>
#include <beholder/commands/ping.h>
#include <beholder/commands/ignoredchannels.h>
#include <beholder/commands/addblock.h>
#include <beholder/commands/scan.h>
#include <filesystem>
#include <fstream>

#include <beholder/botlist.h>
#include <beholder/botlists/topgg.h>
#include <beholder/botlists/discordbotlist.h>
#include <beholder/botlists/infinitybots.h>

size_t tessd_process_count() {
	size_t count = 0;

	for (const auto& task : std::filesystem::directory_iterator("/proc/self/task")) {
		std::ifstream children(task.path() / "children");

		if (!children) {
			continue;
		}

		pid_t pid;

		while (children >> pid) {
			std::ifstream comm("/proc/" + std::to_string(pid) + "/comm");

			if (!comm) {
				continue;
			}

			std::string name;
			std::getline(comm, name);

			if (name == "tessd") {
				count++;
			}
		}
	}

	return count;
}

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
					.set_description(
						"Thank you for inviting **Beholder**! The easiest way to set up the bot is through the "
						"[Beholder Dashboard](https://beholder.cc/dashboard/), where you can configure everything in one place.\n\n"
						"You can also use application commands such as `/logchannel`, `/patterns`, `/ignorechannels` and `/roles`.\n\n"
						"After you've set a log channel, Beholder can read text in screenshots, memes and other images to help stop users bypassing your server's word filters. It can also automatically scan images for NSFW content.\n\n"
						"[Beholder Premium](https://beholder.cc/premium/) adds more powerful image scanning, animated GIF and video scanning, swear and slur filtering, and extra configuration options, all unlimited and for only £3 per month. That's less than a cup of coffee! ☕\n\n"
						"Need help? Join our [support server](https://discord.com/invite/Ac2duhZAr5)."
					)
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

	std::string comma(const std::string& number)
	{
		std::string out;
		out.reserve(number.size() + number.size() / 3);
		auto first = number.size() % 3;
		if (first == 0) {
			first = 3;
		}
		out.append(number.substr(0, first));
		for (size_t i = first; i < number.size(); i += 3) {
			out += ',';
			out.append(number.substr(i, 3));
		}
		return out;
	}

	void on_ready(const dpp::ready_t &event) {
		dpp::cluster& bot = *event.owner;
		if (dpp::run_once<struct register_bot_commands>()) {

			bot.global_bulk_command_create({
				register_command<info_command>(bot),
				register_command<patterns_command>(bot),
				register_command<roles_command>(bot),
				register_command<message_command>(bot),
				register_command<logchannel_command>(bot),
				register_command<ping_command>(bot),
				register_command<ignoredchannels_command>(bot),
				register_command<addblock_command>(bot),
				register_command<scan_command>(bot),
			});

			auto set_presence = [&bot]() {
				db::resultset counts = db::query("SELECT COUNT(*) as guild_total FROM guild_cache");
				std::string guild_count = counts[0].at("guild_total");
				std::string scanned{"0"}, blocked{"0"};
				db::resultset stats = db::query("SELECT SUM(images_scanned) as images_scanned, SUM(images_blocked + images_ocr + images_nsfw) AS images_deleted FROM guild_statistics");
				if (!stats.empty()) {
					scanned = stats[0].at("images_scanned");
					blocked = stats[0].at("images_deleted");
				}
				bot.set_presence(dpp::presence(dpp::ps_online, dpp::at_custom, fmt::format(fmt::runtime("Protecting {} servers. {} images scanned, {} removed."), comma(guild_count), comma(scanned), comma(blocked))));
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
		if (event.created.is_unavailable()) {
			return;
		}
		db::query("INSERT INTO guild_cache (id, owner_id, name, user_count) VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE owner_id = ?, name = ?, user_count = ?", { event.created.id, event.created.owner_id, event.created.name, event.created.member_count, event.created.owner_id, event.created.name, event.created.member_count });
	}

	void on_guild_delete(const dpp::guild_delete_t &event) {
		if (!event.deleted.is_unavailable()) {
			db::query("DELETE FROM guild_cache WHERE id = ?", { event.deleted.id });
			db::query("DELETE FROM channel_settings WHERE guild_id = ?", { event.deleted.id });
			db::query("DELETE FROM guild_config WHERE guild_id = ?", { event.deleted.id });
			db::query("DELETE FROM guild_statistics WHERE guild_id = ?", { event.deleted.id });
			event.owner->log(dpp::ll_info, "Removed from guild: " + event.deleted.id.str());
		}
	}

	using button_handler_t = std::function<void(const dpp::button_click_t &event, const std::vector<std::string>& parts, dpp::cluster& bot, const db::resultset& logchannel)>;

	void on_button_add_block_list(const dpp::button_click_t &event, const std::vector<std::string>& parts, dpp::cluster& bot, const db::resultset& logchannel) {
		/* Block */
		std::string hash = parts[2];
		db::query("INSERT INTO block_list_items (guild_id, hash) VALUES(?,?) ON DUPLICATE KEY UPDATE hash = ?", {event.command.guild_id, hash, hash});
		event.reply(":no_entry: This image has been **added to the block list** by " + event.command.usr.get_mention() + ". It will be **instantly deleted** without performing any further checks.");
	}

	void on_button_remove_block_list(const dpp::button_click_t &event, const std::vector<std::string>& parts, dpp::cluster& bot, const db::resultset& logchannel) {
		/* Unblock */
		std::string hash = parts[2];
		db::query("DELETE FROM block_list_items WHERE guild_id = ? AND hash = ?", {event.command.guild_id, hash});
		event.reply(":white_check_mark: This image has been **removed from the block list** by " + event.command.usr.get_mention() + ". It will now be **checked normally**.");
	}

	void on_button_kick_member(const dpp::button_click_t &event, const std::vector<std::string>& parts, dpp::cluster& bot, const db::resultset& logchannel) {
		/* Kick */
		dpp::snowflake user_id(parts[2]);
		bot.guild_member_kick(event.command.guild_id, user_id, [&bot, parts, event](const auto& cc) {
			if (cc.is_error()) {
				event.reply(":no_entry: Unable to kick user: " + cc.get_error().human_readable);
				return;
			}
			event.reply(":white_check_mark: User <@" + parts[2] + "> has been **kicked** by " + event.command.usr.get_mention() + ".");
		});
	}

	void on_button_timeout_member(const dpp::button_click_t &event, const std::vector<std::string>& parts, dpp::cluster& bot, const db::resultset& logchannel) {
		/* Timeout */
		dpp::snowflake user_id(parts[2]);
		bot.guild_member_timeout(event.command.guild_id, user_id, time(nullptr) + 300, [&bot, parts, event](const auto& cc) {
			if (cc.is_error()) {
				event.reply(":no_entry: Unable to timeout user: " + cc.get_error().human_readable);
				return;
			}
			event.reply(":white_check_mark: User <@" + parts[2] + "> has been **timed out** by " + event.command.usr.get_mention() + " for **five minutes**.");
		});
	}

	void on_button_ban_member(const dpp::button_click_t &event, const std::vector<std::string>& parts, dpp::cluster& bot, const db::resultset& logchannel) {
		/* Ban */
		dpp::snowflake user_id(parts[2]);
		bot.guild_ban_add(event.command.guild_id, user_id, 86400, [&bot, parts, event](const auto& cc) {
			if (cc.is_error()) {
				event.reply(":no_entry: Unable to ban user: " + cc.get_error().human_readable);
				return;
			}
			event.reply(":white_check_mark: User <@" + parts[2] + "> has been **banned** by " + event.command.usr.get_mention() + ". Messages for the past day deleted.");
		});
	}

	void on_button_click(const dpp::button_click_t &event) {
		dpp::permission p = event.command.get_resolved_permission(event.command.usr.id);
		dpp::permission bot_permissions = event.command.app_permissions;

		struct permission_check {
			const dpp::permission permission_user;
			const dpp::permission permission_bot;
			const std::string_view name;
			const std::string_view check;
			const button_handler_t handler;
		};

		const std::map<std::string_view, permission_check> permissions_per_section = {
			{"BL", {dpp::p_manage_guild, 0, "manage server", "add images to the block list", on_button_add_block_list}},
			{"UB", {dpp::p_manage_guild, 0, "manage server", "remove images from the block list", on_button_remove_block_list}},
			{"KI", {dpp::p_kick_members, dpp::p_kick_members, "kick members", "kick members from the server", on_button_kick_member}},
			{"TI", {dpp::p_moderate_members, dpp::p_moderate_members, "moderate members", "timeout members", on_button_timeout_member}},
			{"BA", {dpp::p_ban_members, dpp::p_ban_members, "ban members", "ban members from the server", on_button_ban_member}},
		};

		dpp::cluster& bot = *(event.owner);
		std::vector<std::string> parts = dpp::utility::tokenize(event.custom_id, ";");
		bot.log(dpp::ll_info, "Button click with id: " + event.custom_id);
		db::resultset logchannel = db::query("SELECT embeds_disabled, log_channel, embed_title, embed_body FROM guild_config WHERE guild_id = ?", { event.command.guild_id });
		if (!logchannel.size() || parts.size() < 3) {
			event.reply(dpp::message(event.command.channel_id, "There's something weird about this button, or your server's configuration is missing.").set_flags(dpp::m_ephemeral));
			return;
		}
		auto check = permissions_per_section.find(parts[0]);
		if (check == permissions_per_section.end()) {
			event.reply(dpp::message(event.command.channel_id, "Missing permissions information in the bot for this interaction. This is a bug, please contact the developer.").set_flags(dpp::m_ephemeral));
			return;
		} else if (check->second.permission_user && !p.has(check->second.permission_user)) {
			event.reply(dpp::message(event.command.channel_id, "You require the " + std::string(check->second.name) + " permission to " + std::string(check->second.check)).set_flags(dpp::m_ephemeral));
			return;
		} else if (check->second.permission_bot && !bot_permissions.has(check->second.permission_bot)) {
			event.reply(dpp::message(event.command.channel_id, "I do not have the " + std::string(check->second.name) + " permission to " + std::string(check->second.check)).set_flags(dpp::m_ephemeral));
			return;
		}
		check->second.handler(event, parts, bot, logchannel);
	}

	void on_slashcommand(const dpp::slashcommand_t &event) {
		event.owner->log(
			dpp::ll_info,
			fmt::format(
				fmt::runtime("COMMAND: {} by {} ({} Guild: {})"),
				event.command.get_command_name(),
				event.command.usr.format_username(),
				event.command.usr.id,
				event.command.guild_id
			)
		);
		route_command(event);
	}

	void on_message_update(const dpp::message_update_t& event) {
		if (event.msg.author.is_bot() || event.msg.author.id.empty()) {
			return;
		}

		constexpr double one_week = 7 * 24 * 60 * 60;
		if (dpp::utility::time_f() - event.msg.id.get_creation_time() > one_week) {
			event.owner->log(dpp::ll_debug, "Dropped message edit " + event.msg.id.str() + " older than one week");
			return;
		}

		try {
			dpp::json j = dpp::json::parse(event.raw_event).at("d");
			if (!j.contains("member") || !j["member"].is_object()) {
				event.owner->log(dpp::ll_debug, "Dropped message edit " +event.msg.id.str() + " without member details");
				return;
			}
			if (!j["member"].contains("roles") || !j["member"]["roles"].is_array()) {
				event.owner->log(dpp::ll_debug, "Dropped message edit " +event.msg.id.str() + " without member role list");
				return;
			}
		}
		catch (const std::exception &e) {
			event.owner->log(dpp::ll_warning, "Dropped message edit " +event.msg.id.str() + " with invalid raw JSON");
			return;
		}

		/* Message update is mapped to message creation.
		 * The effect is the same, the message is simply re-scanned.
		 */
		dpp::message_create_t c(event.owner, event.shard, event.raw_event);
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
			if (ping.first.id == event.owner->me.id) {
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
				download_image(attach, *event.owner, event);
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

		std::set<std::string> seen_urls;
		/* Check each word in the message looking for URLs */
		for (std::string& possibly_url : parts) {
			std::string original_url = possibly_url;
			possibly_url = dpp::lowercase(possibly_url);

			size_t url_pos = possibly_url.find("<http");
			size_t offset = 1;
			char closing_wrapper = '>';

			if (url_pos == std::string::npos) {
				url_pos = possibly_url.find("[http");
				offset = 1;
				closing_wrapper = ']';
			}

			if (url_pos == std::string::npos) {
				url_pos = possibly_url.find("http");
				offset = 0;
				closing_wrapper = '\0';
			}

			if (url_pos == std::string::npos) {
				continue;
			}

			possibly_url = possibly_url.substr(url_pos + offset);
			original_url = original_url.substr(url_pos + offset);

			if (closing_wrapper != '\0') {
				size_t wrapper_end = possibly_url.find(closing_wrapper);

				if (wrapper_end != std::string::npos) {
					possibly_url = possibly_url.substr(0, wrapper_end);
					original_url = original_url.substr(0, wrapper_end);
				}
			}

			if (!seen_urls.insert(original_url).second) {
				continue;
			}

			dpp::attachment attach((dpp::message*)&event.msg);
			attach.url = original_url;

			try {
				Url u(original_url);
				attach.filename = fs::path(u.path()).filename();
			}
			catch (const std::exception&) {
				attach.filename = fs::path(original_url).filename();
			}

			download_image(attach, *event.owner, event);
		}
	}
}

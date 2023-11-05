#include <fmt/format.h>
#include <beholder/listeners.h>
#include <beholder/database.h>
#include <beholder/beholder.h>
#include <beholder/command.h>

#include <beholder/commands/logchannel.h>
#include <beholder/commands/roles.h>
#include <beholder/commands/message.h>
#include <beholder/commands/patterns.h>
#include <beholder/commands/premium.h>
#include <beholder/commands/info.h>
#include <beholder/commands/ping.h>

namespace listeners {

	void on_ready(const dpp::ready_t &event) {
		dpp::cluster& bot = *event.from->creator;
		if (dpp::run_once<struct register_bot_commands>()) {
			uint64_t default_permissions = dpp::p_administrator | dpp::p_manage_guild;
			bot.global_bulk_command_create({
				register_command<info_command>(bot),
				register_command<premium_command>(bot),
				register_command<patterns_command>(bot),
				register_command<roles_command>(bot),
				register_command<message_command>(bot),
				register_command<logchannel_command>(bot),
				register_command<ping_command>(bot),
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
			set_presence();
		}
	}


	void on_slashcommand(const dpp::slashcommand_t &event) {
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
		bool should_bypass = false;

		/* If the author is a bot, stop the event (no checking). */
		if (event.msg.author.is_bot()) {
			return;
		}

		for (const auto& ping : event.msg.mentions) {
			if (ping.first.id == event.from->creator->me.id) {
				sarcastic_ping(event);
				break;
			}
		}

		/* Loop through all bypass roles in database */
		db::resultset bypass_roles = db::query("SELECT * FROM guild_bypass_roles WHERE guild_id = '?'", { event.msg.guild_id.str() });
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

		/* Split the message by spaces. */
		std::vector<std::string> parts = dpp::utility::tokenize(event.msg.content, " ");

		/* Check for images in the embeds of the message, if any. Found urls and
		 * scannable content are appended into the parts vector for scanning as text content.
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
				auto spaced = dpp::utility::tokenize(embed.description, " ");
				if (!spaced.empty()) {
					parts.insert(parts.end(), spaced.begin(), spaced.end());
				}
				for (const dpp::embed_field& field : embed.fields) {
					auto spaced = dpp::utility::tokenize(field.value, " ");
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

		/* Check each word in the message looking for URLs */
		for (std::string& possibly_url : parts) {
			size_t size = possibly_url.length();
			possibly_url = dpp::lowercase(possibly_url);
			/* Check for markdown cloaked urls: [descr potentially with spaces](url) */
			auto cloaked_url_pos = possibly_url.find("](http");
			if (cloaked_url_pos != std::string::npos && possibly_url.length() - cloaked_url_pos - 3 > 7) {
				possibly_url = possibly_url.substr(cloaked_url_pos + 2, possibly_url.length() - cloaked_url_pos - 3);
			}
			if (possibly_url.substr(0, 19) == "https://images-ext-" && possibly_url.find("cdn.discordapp.com") != std::string::npos) {
				/* Proxy url to something on discord cdn, skip as we would just get a 401 */
				continue;
			}
			if ((size >= 9 && possibly_url.substr(0, 8) == "https://") ||
			(size >= 8 && possibly_url.substr(0, 7) == "http://")) {
				dpp::attachment attach((dpp::message*)&event.msg);
				attach.url = possibly_url;
				/* Strip off query parameters */
				auto pos = possibly_url.find('?');
				if (pos != std::string::npos) {
					possibly_url = possibly_url.substr(0, pos);
				}
				attach.filename = fs::path(possibly_url).filename();
				download_image(attach, *event.from->creator, event);
			}
		}
	}
}

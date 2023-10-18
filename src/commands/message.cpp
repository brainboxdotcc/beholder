#include <beholder/beholder.h>
#include <beholder/database.h>
#include <beholder/commands/message.h>
#include <beholder/database.h>

dpp::slashcommand message_command::register_command(dpp::cluster& bot)
{
	bot.on_form_submit([&bot](const dpp::form_submit_t& event) {
		if (event.custom_id == "set_embed_modal") {
			std::string embed_title = std::get<std::string>(event.components[0].components[0].value);
			std::string embed_body = std::get<std::string>(event.components[1].components[0].value);
			db::query(
				"INSERT INTO guild_config (guild_id, embed_title, embed_body) VALUES('?', '?', '?') ON DUPLICATE KEY UPDATE embed_title = '?', embed_body = '?'",
				{ event.command.guild_id.str(), embed_title, embed_body, embed_title, embed_body }
			);
			/* Replace @user with the user's mention for preview */
			embed_body = replace_string(embed_body, "@user", "<@" + event.command.usr.id.str() + ">");
			event.reply(
				dpp::message("✅ Delete message set.\n\n**__Preview:__**")
					.set_flags(dpp::m_ephemeral)
					.add_embed(dpp::embed().set_description(embed_body).set_title(embed_title).set_color(0xff7a7a))
			);
		}
	});

	return dpp::slashcommand("message", "Set the details of the embed to send when images have forbidden text", bot.me.id)
		.set_default_permissions(dpp::p_administrator | dpp::p_manage_guild);
}


void message_command::route(const dpp::slashcommand_t &event)
{
	db::resultset embed = db::query("SELECT embed_body, embed_title FROM guild_config WHERE guild_id = '?'", { event.command.guild_id.str() });
	std::string embed_body, embed_title;
	if (!embed.empty()) {
		embed_body = embed[0].at("embed_body");
		embed_title = embed[0].at("embed_title");
	}

	dpp::interaction_modal_response modal("set_embed_modal", "Enter delete message details");
	/* Add a text component */
	modal.add_component(
		dpp::component()
			.set_label("Message Title")
			.set_id("title")
			.set_type(dpp::cot_text)
			.set_placeholder("Enter the title of the message")
			.set_default_value(embed_title)
			.set_min_length(1)
			.set_max_length(256)
			.set_required(true)
			.set_text_style(dpp::text_short)
	);
	modal.add_row();
	modal.add_component(
		dpp::component()
			.set_label("Message Body")
			.set_id("body")
			.set_type(dpp::cot_text)
			.set_placeholder("Enter body of message. You can use the word '@user' here which will be replaced with a mention of the user who triggered the message.")
			.set_default_value(embed_body)
			.set_min_length(1)
			.set_max_length(3800)
			.set_required(true)
			.set_text_style(dpp::text_paragraph)
	);
	event.dialog(modal);
}
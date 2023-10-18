#pragma once
#include <beholder/beholder.h>
#include <beholder/command.h>

struct message_command : public command {
	static constexpr std::string_view name{"message"};
	static dpp::slashcommand register_command(dpp::cluster& bot);
	static void route(const dpp::slashcommand_t &event);
};
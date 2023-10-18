#pragma once
#include <beholder/beholder.h>
#include <beholder/command.h>

struct roles_command : public command {
	static constexpr std::string_view name{"roles"};
	static dpp::slashcommand register_command(dpp::cluster& bot);
	static void route(const dpp::slashcommand_t &event);
};
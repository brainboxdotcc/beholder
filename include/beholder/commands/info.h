#pragma once
#include <beholder/beholder.h>
#include <beholder/command.h>

struct info_command : public command {
	static constexpr std::string_view name{"info"};
	static dpp::slashcommand register_command(dpp::cluster& bot);
	static void route(const dpp::slashcommand_t &event);
};
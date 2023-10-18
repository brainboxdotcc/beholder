#pragma once
#include <beholder/beholder.h>
#include <beholder/command.h>

struct patterns_command : public command {
	static constexpr std::string_view name{"patterns"};
	static dpp::slashcommand register_command(dpp::cluster& bot);
	static void route(const dpp::slashcommand_t &event);
};
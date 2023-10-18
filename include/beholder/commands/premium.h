#pragma once
#include <beholder/beholder.h>
#include <beholder/command.h>

struct premium_command : public command {
	static constexpr std::string_view name{"premium"};
	static dpp::slashcommand register_command(dpp::cluster& bot);
	static void route(const dpp::slashcommand_t &event);
};
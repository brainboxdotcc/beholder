#pragma once
#include <beholder/beholder.h>
#include <beholder/command.h>

struct logchannel_command : public command {
	static constexpr std::string_view name{"logchannel"};
	static dpp::slashcommand register_command(dpp::cluster& bot);
	static void route(const dpp::slashcommand_t &event);
};
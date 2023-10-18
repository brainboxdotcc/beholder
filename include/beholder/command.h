#pragma once
#include <dpp/dpp.h>
#include <beholder/beholder.h>

using command_router = auto (*)(const dpp::slashcommand_t&) -> void;

struct command {
	static constexpr std::string_view name{};
	inline static dpp::slashcommand register_command(dpp::cluster& bot)
	{
		throw new std::runtime_error("Don't call the base command class!");
	}

	inline static void route(const dpp::slashcommand_t &event)
	{
		throw new std::runtime_error("Don't call the base command class!");
	}
};

std::unordered_map<std::string_view, command_router>& get_command_map();

template <typename T> dpp::slashcommand register_command(dpp::cluster& bot)
{
	auto& command_map = get_command_map();
	command_map[T::name] = T::route;
	return T::register_command(bot);
}



void route_command(const dpp::slashcommand_t &event);

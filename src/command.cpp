#include <dpp/dpp.h>
#include <beholder/command.h>

static std::unordered_map<std::string_view, command_router> registered_commands;

std::unordered_map<std::string_view, command_router>& get_command_map()
{
	return registered_commands;
}

void route_command(const dpp::slashcommand_t &event)
{
	auto ref = registered_commands.find(event.command.get_command_name());
	if (ref != registered_commands.end()) {
		auto ptr = ref->second;
		(*ptr)(event);
	}
}
#include <dpp/dpp.h>
#include <beholder/command.h>

/**
 * @brief Internal command map
 */
static registered_command_list registered_commands;

registered_command_list& get_command_map()
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

#include <dpp/dpp.h>
#include <beholder/command.h>
#include <beholder/sentry.h>

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
		std::thread([ptr, event]() {
			void *slashlog = sentry::start_transaction(sentry::register_transaction_type("/" + event.command.get_command_name(), "event.slashcommand"));
			sentry::set_user(event.command.get_issuing_user());
			(*ptr)(event);
			sentry::end_transaction(slashlog);
			sentry::unset_user();
		}).detach();
	}
}

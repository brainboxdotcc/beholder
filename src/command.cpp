/************************************************************************************
 * 
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
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
			sentry::set_user(event.command.get_issuing_user(), event.command.guild_id);
			(*ptr)(event);
			sentry::end_transaction(slashlog);
			sentry::unset_user();
		}).detach();
	} else {
		event.from->creator->log(dpp::ll_error, "Unable to route command: " + event.command.get_command_name());
	}
}

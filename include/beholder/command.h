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
#pragma once
#include <dpp/dpp.h>
#include <beholder/beholder.h>

/**
 * @brief All commands derive from this struct. Note that these structs are never
 * instantiated, all data witin them is static and constant. Register your commands
 * by calling the register_command function to get a dpp::slashcommand you can then
 * pass to global_bulk_command_create:
 * 
 * bot.global_bulk_command_create({
 * 	register_command<my_command>(bot),
 * });
 */
struct command {
	/**
	 * @brief Change the name to that of your command; this is used
	 * to route your command in the on_slashcommand event.
	 */
	static constexpr std::string_view name{};

	/**
	 * @brief Register your command.
	 * This function should set up any event handlers your command needs to
	 * function, and return the dpp::slashcommand we should register.
	 * 
	 * @param bot Reference to the cluster registering the command
	 * @return dpp::slashcommand slash command to register
	 */
	static dpp::slashcommand register_command(dpp::cluster& bot);

	/**
	 * @brief Handle slash command
	 * This is called when a user issues your command.
	 * 
	 * @param event The slash command event data
	 */
	static void route(const dpp::slashcommand_t &event);
};

/**
 * @brief A function pointer to the static route() function of a command
 */
using command_router = auto (*)(const dpp::slashcommand_t&) -> void;

/**
 * @brief Represents a list of registered commands stored in an unordered_map
 */
using registered_command_list = std::unordered_map<std::string_view, command_router>;

/**
 * @brief Get the current map of registered commands
 * 
 * @return registered_command_list& the list
 */
registered_command_list& get_command_map();

/**
 * @brief Register a command and return its slashcommand object.
 * 
 * @tparam T command handler struct to register
 * @param bot Reference to cluster registering the command
 * @return dpp::slashcommand slashcommand object to register
 */
template <typename T> dpp::slashcommand register_command(dpp::cluster& bot)
{
	auto& command_map = get_command_map();
	command_map[T::name] = T::route;
	return T::register_command(bot);
}

/**
 * @brief Called by the on_slashcommand event to route a command by name
 * to its handler.
 * 
 * @param event Slash command interaction event
 */
void route_command(const dpp::slashcommand_t &event);

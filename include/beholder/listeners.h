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

/**
 * @brief Event listeners for DPP events
 */
namespace listeners {
	/**
	 * @brief handle shard ready
	 * 
	 * @param event ready_t
	 */
	void on_ready(const dpp::ready_t &event);

	/**
	 * @brief handle slash command
	 * 
	 * @param event slashcommand_t
	 */
	void on_slashcommand(const dpp::slashcommand_t& event);

	/**
	 * @brief handle message creation
	 * 
	 * @param event message_create_t
	 */
	void on_message_create(const dpp::message_create_t& event);

	/**
	 * @brief handle message editing
	 * 
	 * @param event message_update_t
	 */
	void on_message_update(const dpp::message_update_t &event);
};

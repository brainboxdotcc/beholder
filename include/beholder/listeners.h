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

#pragma once

#include <dpp/json_fwd.h>
#include <beholder/beholder.h>

namespace config {

	/**
	 * @brief Initialise config file
	 * 
	 * @param config_file Config file to read
	 */
	void init(const std::string& config_file);

	/**
	 * @brief Get all config values from a specific key
	 * 
	 * @param key The key, if empty/omitted the root node is returned
	 * @return json& configuration or empty container if not found
	 */
	json& get(const std::string& key = "");

};
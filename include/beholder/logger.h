#pragma once
#include <dpp/dpp.h>

namespace logger {

	/**
	 * @brief Initialise spdlog logger
	 * 
	 * @param log_file log file to log to as well as stdout.
	 * stderr is closed to silence spurious output from libleptonica.
	 * The logs will be rotated automatically.
	 */
	void init(const std::string& log_file);

	/**
	 * @brief Point dpp::cluster::on_log at this function
	 * 
	 * @param event dpp::log_t event
	 */
	void log(const dpp::log_t & event);

};
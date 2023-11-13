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
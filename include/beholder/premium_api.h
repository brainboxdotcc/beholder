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

namespace premium_api {

	/**
	 * @brief Mapping of model to cache table
	 */
	struct model_mapping {
		std::string_view model;
		std::string_view table_suffix;
	};

	bool find_banned_type(const json& response, const dpp::attachment& attach, dpp::cluster& bot, const dpp::message_create_t ev, const std::string& content);

	bool perform_api_scan(int pass, bool &flattened, const std::string& hash, std::string& file_content, const dpp::attachment& attach, dpp::cluster& bot, const dpp::message_create_t ev);

	void report(dpp::cluster& bot, bool is_good, dpp::snowflake message_id, dpp::snowflake channel_id, const std::string& image_url, const std::string& model);
};

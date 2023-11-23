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
#include <beholder/beholder.h>
#include <beholder/botlist.h>

/**
 * @brief top.gg botlist (formerly DBL)
 */
struct topgg : public botlist {
	static constexpr std::string_view name{"top.gg"};
	static constexpr std::string_view url{"https://top.gg/api/bots/{}/stats"};
	static constexpr std::string_view server_count_field{"server_count"};
	static constexpr std::string_view shard_count_field{"shard_count"};

	static void post(dpp::cluster& bot) {  
		botlist::run(bot, topgg::name, topgg::url, topgg::server_count_field, topgg::shard_count_field);
	}
};

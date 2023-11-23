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
 * @brief Represents a bot list site.
 * @note As this is all static constant data, these structs are never instantiated.
 * They are simply called statically.
 */
struct botlist {
	/**
	 * @brief The name of the bot list site. Used as the key under the "botlists"
	 * key in the configuration file.
	 */
	static constexpr std::string_view name{};

	/**
	 * @brief URL format to use, {} will be replaced with the bot's application ID.
	 */
	static constexpr std::string_view url{};

	/**
	 * @brief Field name to use in JSON POST data to hold the count of servers.
	 * If set to an empty string, no server count will be sent.
	 */
	static constexpr std::string_view server_count_field{};
	
	/**
	 * @brief Field name to use in JSON POST data to hold the count of shards.
	 * If set to an empty string, no shard count will be sent.
	 * 
	 */
	static constexpr std::string_view shard_count_field{};

	/**
	 * @brief Handle posting an update to the bot list site
	 * @note There is no implementation of this base struct function.
	 * This is to prevent accidental instantiation of the base struct.
	 * 
	 * @param bot reference to D++ cluster
	 */
	static void post(dpp::cluster& bot);

protected:
	/**
	 * @brief Perform the POST to the bot list website asynchronously.
	 * Will complete in the background, this is fire-and-forget.
	 * On error, an error will be logged to the logger. Should be called
	 * by a botlist::post() implementation.
	 * 
	 * @param bot cluster reference
	 * @param key configuration key name
	 * @param url bot list url formatter
	 * @param count_field count field in postdata
	 * @param shards_field shards field in postdata
	 */
	static void run(dpp::cluster& bot, const std::string_view key, const std::string_view url, const std::string_view count_field, const std::string_view shards_field);
};

/**
 * @brief Represents the botlist::post() function
 */
using botlist_router = auto (*)(dpp::cluster&) -> void;

/**
 * @brief Represents a list of registered botlists stored in an unordered_map
 */
using registered_botlist_list = std::unordered_map<std::string_view, botlist_router>;

registered_botlist_list& get_botlist_map();

/**
 * @brief Register a botlist
 * 
 * @tparam T botlist handler struct to register
 */
template <typename T> void register_botlist()
{
	auto& botlist_map = get_botlist_map();
	botlist_map[T::name] = T::post;
}

/**
 * @brief Request that all botlists are updated
 * 
 * @param bot cluster reference
 */
void post_botlists(dpp::cluster &bot);

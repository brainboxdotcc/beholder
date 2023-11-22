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
#include <atomic>
#include <cstdint>

namespace fs = std::filesystem;

#define BEHOLDER_VERSION "beholder@1.0.0"

constexpr size_t max_size = 8 * 1024 * 1024;
constexpr int max_concurrency = 12;

namespace colours {
	constexpr uint32_t bad = 0xff7a7a;
	constexpr uint32_t good = 0x7aff7a;
};

#define INCREMENT_STATISTIC(STAT_NAME, GUILD_ID) \
		db::query("INSERT INTO guild_statistics (guild_id, stat_date, " STAT_NAME ") VALUES(?,NOW(),1) ON DUPLICATE KEY UPDATE " STAT_NAME " = " STAT_NAME " + 1", { GUILD_ID });

#define INCREMENT_STATISTIC2(STAT_NAME1, STAT_NAME2, GUILD_ID) \
		db::query("INSERT INTO guild_statistics (guild_id, stat_date, " STAT_NAME1 ", " STAT_NAME2 ") VALUES(?,NOW(),1,1) ON DUPLICATE KEY UPDATE " STAT_NAME1 " = " STAT_NAME1 " + 1, " STAT_NAME2 " = " STAT_NAME2 " + 1", { GUILD_ID });

extern std::atomic<int> concurrent_images;

using json = dpp::json;

bool match(const char* str, const char* mask);

/**
 *  trim from end of string (right)
 */
inline std::string rtrim(std::string s)
{
	s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1);
	return s;
}

/**
 * trim from beginning of string (left)
 */
inline std::string ltrim(std::string s)
{
	s.erase(0, s.find_first_not_of(" \t\n\r\f\v"));
	return s;
}

/**
 * trim from both ends of string (right then left)
 */
inline std::string trim(std::string s)
{
	return ltrim(rtrim(s));
}

/**
 * @brief Processes an attachment into text, then checks to see if it matches a certain pattern. If it matches then it called delete_message_and_warn.
 * @param attach The attachment to process into text.
 * @param bot Bot reference.
 * @param ev message_create_t reference.
 */
void download_image(const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev);

/**
 * @brief Delete a message and send a warning.
 * @param bot Bot reference.
 * @param ev message_create_t reference.
 * @param attach The attachment that was flagged as bad.
 * @param text What the attachment was flagged for.
 * @param premium prefer premium message
 * @param trigger premium trigger threshold
 */
void delete_message_and_warn(const std::string& hash, const std::string& image, dpp::cluster& bot, const dpp::message_create_t ev, const dpp::attachment attach, const std::string text, bool premium, double trigger = 0.0, double threshold = 0.0);

std::string replace_string(std::string subject, const std::string& search, const std::string& replace);

std::string sha256(const std::string &buffer);

void on_thread_exit(std::function<void()> func);
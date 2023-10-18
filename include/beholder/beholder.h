#pragma once
#include <dpp/dpp.h>

constexpr size_t max_size = 8 * 1024 * 1024;
constexpr int max_concurrency = 64;

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

void good_embed(dpp::cluster &bot, dpp::snowflake channel_id, const std::string &message, const std::string& url = "");
void bad_embed(const std::string& title, dpp::cluster &bot, dpp::snowflake channel_id, const std::string &message, dpp::message ref = {});

/**
 * @brief Processes an attachment into text, then checks to see if it matches a certain pattern. If it matches then it called delete_message_and_warn.
 * @param attach The attachment to process into text.
 * @param bot Bot reference.
 * @param ev message_create_t reference.
 */
void download_image(const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev);

void ocr_image(std::string file_content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev);

/**
 * @brief Delete a message and send a warning.
 * @param bot Bot reference.
 * @param ev message_create_t reference.
 * @param attach The attachment that was flagged as bad.
 * @param text What the attachment was flagged for.
 * @param premium prefer premium message
 */
void delete_message_and_warn(dpp::cluster& bot, const dpp::message_create_t ev, const dpp::attachment attach, const std::string text, bool premium);

std::string replace_string(std::string subject, const std::string& search, const std::string& replace);

bool find_banned_type(const json& response, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev);
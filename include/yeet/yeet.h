#pragma once
#include <dpp/dpp.h>

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
void bad_embed(dpp::cluster &bot, dpp::snowflake channel_id, const std::string &message, dpp::message ref = {});
void good_embed(dpp::commandhandler &ch, dpp::command_source src,  const std::string &message);
void bad_embed(dpp::commandhandler &ch, dpp::command_source src, const std::string &message);


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
#include <dpp/dpp.h>
#include <beholder/beholder.h>
#include <iomanip>
#include <string>

bool match(const char* str, const char* mask)
{
	char* cp = NULL;
	char* mp = NULL;
	char* string = (char*)str;
	char* wild = (char*)mask;

	while ((*string) && (*wild != '*')) {
		if ((tolower(*wild) != tolower(*string)) && (*wild != '?')) {
			return 0;
		}
		wild++;
		string++;
	}

	while (*string) {
		if (*wild == '*') {
			if (!*++wild) {
				return 1;
			}
			mp = wild;
			cp = string+1;
		}
		else {
			if ((tolower(*wild) == tolower(*string)) || (*wild == '?')) {
				wild++;
				string++;
			} else {
				wild = mp;
				string = cp++;
			}
		}
	}

	while (*wild == '*') {
		wild++;
	}

	return !*wild;
}

std::string replace_string(std::string subject, const std::string& search, const std::string& replace)
{
        size_t pos = 0;

        std::string subject_lc = dpp::lowercase(subject);
        std::string search_lc = dpp::lowercase(search);
        std::string replace_lc = dpp::lowercase(replace);

        while ((pos = subject_lc.find(search_lc, pos)) != std::string::npos) {

                 subject.replace(pos, search.length(), replace);
                 subject_lc.replace(pos, search_lc.length(), replace_lc);

                 pos += replace.length();
        }
        return subject;
}

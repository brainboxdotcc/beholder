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

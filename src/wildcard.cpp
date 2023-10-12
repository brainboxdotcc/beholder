#include <yeet/yeet.h>
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
 
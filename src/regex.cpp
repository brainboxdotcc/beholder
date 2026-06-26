#include <beholder/regex.h>
#include <array>
#include <cstring>

namespace regex {

regex_exception::regex_exception(const std::string& message) : message(message) {
}

const char* regex_exception::what() const noexcept {
    return message.c_str();
}

regex::regex(const std::string& pattern, bool case_insensitive) {
    int flags = REG_EXTENDED;

    if (case_insensitive) {
        flags |= REG_ICASE;
    }

    int result = tre_regcomp(&compiled, pattern.c_str(), flags);

    if (result != 0) {
        char buffer[256];
        tre_regerror(result, &compiled, buffer, sizeof(buffer));
        throw regex_exception(buffer);
    }
}

regex::~regex() {
    tre_regfree(&compiled);
}

bool regex::match(const std::string& comparison) const {
    return tre_regexec(&compiled, comparison.c_str(), 0, nullptr, 0) == 0;
}

bool regex::match(const std::string& comparison, std::vector<std::string>& matches) const {
    matches.clear();

    std::array<regmatch_t, 32> capture;
    int result = tre_regexec(&compiled, comparison.c_str(), capture.size(), capture.data(), 0);

    if (result == REG_NOMATCH) {
        return false;
    }

    if (result != 0) {
        char buffer[256];
        tre_regerror(result, &compiled, buffer, sizeof(buffer));
        throw regex_exception(buffer);
    }

    for (const auto& m : capture) {
        if (m.rm_so == -1) {
            break;
        }

        matches.emplace_back(
            comparison.substr(
                static_cast<std::size_t>(m.rm_so),
                static_cast<std::size_t>(m.rm_eo - m.rm_so)
            )
        );
    }

    return true;
}

}

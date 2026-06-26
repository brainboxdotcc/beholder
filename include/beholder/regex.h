#pragma once

#include <exception>
#include <tre/tre.h>
#include <string>
#include <vector>

namespace regex {

class regex_exception : public std::exception {
public:
    explicit regex_exception(const std::string& message);

    const char* what() const noexcept override;

private:
    std::string message;
};

class regex {
    regex_t compiled;

public:
    regex(const std::string& pattern, bool case_insensitive = false);
    ~regex();

    regex(const regex&) = delete;
    regex& operator=(const regex&) = delete;

    bool match(const std::string& comparison) const;
    bool match(const std::string& comparison, std::vector<std::string>& matches) const;
};

}


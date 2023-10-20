#pragma once

#include <dpp/json_fwd.h>
#include <beholder/beholder.h>

namespace config {

	void init();

	json& get(const std::string& key = "");

};
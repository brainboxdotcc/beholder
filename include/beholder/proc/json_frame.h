#pragma once

#include <dpp/json.h>
#include <iostream>
#include <string>
#include <string_view>

namespace proc {

	inline constexpr std::string_view json_marker = "__BEHOLDER_JSON__";

	inline void write_frame(std::ostream& output, const dpp::json& frame)
	{
		output << json_marker << frame.dump() << "\n" << std::flush;
	}

	inline void write_frame(const dpp::json& frame)
	{
		write_frame(std::cout, frame);
	}

	inline bool read_frame(std::istream& input, dpp::json& frame)
	{
		std::string line;

		while (std::getline(input, line)) {
			const std::size_t marker_pos = line.find(json_marker);

			if (marker_pos != std::string::npos) {
				line = line.substr(marker_pos + json_marker.length());
			}

			try {
				frame = dpp::json::parse(line);
				return true;
			} catch (const dpp::json::exception&) {
				continue;
			}
		}

		return false;
	}

}
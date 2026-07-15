/************************************************************************************
 *
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023,2026 Craig Edwards <support@sporks.gg>
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

#include <dpp/json.h>
#include <iostream>
#include <string>
#include <string_view>
#include <array>

namespace proc {

	/**
	 * @brief Marker prefix identifying JSON protocol messages.
	 *
	 * This allows JSON messages to be distinguished from ordinary stdout
	 * produced by child processes. Any output not prefixed with this marker
	 * is ignored by the protocol parser.
	 */
	inline constexpr std::string_view json_marker = "__BEHOLDER_JSON__";

	/**
	 * @brief Construct a framed JSON message at compile time.
	 *
	 * Prepends the protocol marker to a JSON string literal and appends the
	 * terminating null byte. This is primarily intended for use in contexts
	 * where dynamic allocation is undesirable, such as signal handlers.
	 *
	 * @tparam N Size of the JSON string literal, including the null terminator.
	 * @param json JSON string literal.
	 * @return Compile-time character array containing the complete framed message.
	 */
	template<std::size_t N> consteval auto make_json_frame(const char (&json)[N])
	{
		std::array<char, proc::json_marker.size() + N> frame{};

		std::size_t pos = 0;

		for (char c : proc::json_marker) {
			frame[pos++] = c;
		}

		for (std::size_t i = 0; i < N; ++i) {
			frame[pos++] = json[i];
		}

		return frame;
	}

	/**
	 * @brief Write a framed JSON message to an output stream.
	 *
	 * The message is prefixed with the protocol marker and terminated with
	 * a newline to allow multiple messages to be transmitted over the same
	 * stream.
	 *
	 * @param output Destination stream.
	 * @param frame JSON object to write.
	 */
	inline void write_frame(std::ostream& output, const dpp::json& frame)
	{
		output << json_marker << frame.dump() << "\n" << std::flush;
	}

	/**
	 * @brief Write a framed JSON message to stdout.
	 *
	 * @param frame JSON object to write.
	 */
	inline void write_frame(const dpp::json& frame)
	{
		write_frame(std::cout, frame);
	}

	/**
	 * @brief Read the next framed JSON message from a stream.
	 *
	 * Lines which are not valid JSON are ignored. If the protocol marker is
	 * present anywhere within a line, all preceding text is discarded before
	 * parsing. This allows child processes to emit ordinary logging without
	 * disrupting the protocol.
	 *
	 * @param input Source stream.
	 * @param frame Receives the decoded JSON object.
	 * @return True if a JSON message was successfully read, otherwise false.
	 */
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
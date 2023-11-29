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
#pragma once
#include <dpp/dpp.h>
#include <beholder/ocr.h>
#include <beholder/tensorflow_api.h>
#include <beholder/premium_api.h>
#include <beholder/label.h>

namespace image {


	using scanner_function = auto (*)(bool&, const std::string&, std::string&, const dpp::attachment&, dpp::cluster&, const dpp::message_create_t, int, bool) -> bool;

	constexpr inline std::array<const char*, 4> scanner_names{
		"Text Recognition Rules",
		"Basic NSFW Rules",
		"Premium NSFW Rules",
		"Image Label Rules",
	};

	constexpr inline std::array<scanner_function, 4> scanners{
		ocr::scan,
		tensorflow_api::scan,
		premium_api::scan,
		label::scan,
	};	

	std::string flatten_gif(dpp::cluster& bot, const dpp::attachment attach, std::string file_content);

	void worker_thread(std::string file_content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev);
}

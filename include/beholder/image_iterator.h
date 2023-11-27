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
#include <vector>
#include <string>
#include <functional>

namespace image {

	using on_image = std::function<bool(std::string, std::string, dpp::message_context_menu_t)>;
	using on_end = std::function<void(std::vector<std::string>, size_t)>;

	void iterate(const dpp::message msg, dpp::message_context_menu_t event, on_image image_event, on_end end_event);
}

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
#include <beholder/database.h>
#include <beholder/premium_api.h>
#include <beholder/ocr.h>
#include <beholder/config.h>
#include <beholder/proc/cpipe.h>
#include <beholder/proc/spawn.h>
#include <beholder/image.h>
#include <dpp/json.h>
#include "3rdparty/httplib.h"
#include <fmt/format.h>
#include <CxxUrl/url.hpp>

namespace premium_api {

	/**
	 * @brief Given two objects, deep copy them together into one.
	 * nlohmann::json::update() and nlohmann::json::merge_patch() don't do this,
	 * and only copy shallow.
	 * 
	 * @param lhs left hand side, will have keys replaced
	 * @param rhs right hand side, has priority
	 * @return nlohmann::json return value
	 */
	nlohmann::json object_merge(const json & lhs, const json & rhs) {
		// start with the full LHS json, into which we'll copy everything from RHS
		json j = lhs;
		// if a key exists in both LHS and RHS, then we keep RHS
		for (auto it = rhs.cbegin(); it != rhs.cend(); ++it) {
			const auto & key = it.key();
			if (it->is_object()) {
				if (lhs.contains(key)) {
					// object already exists (must merge)
					j[key] = object_merge(lhs[key], rhs[key]);
				} else {
					// object does not exist in LHS, so use the RHS
					j[key] = rhs[key];
				}
			} else {
				// this is an individual item, use RHS
				j[key] = it.value();
			}
		}
		return j;
	}

	bool find_banned_type(const std::string& hash, const json& response, const dpp::attachment& attach, dpp::cluster& bot, const dpp::message_create_t ev, const std::string& content)
	{
		db::resultset premium_filters = db::query("SELECT * FROM premium_filters WHERE guild_id = ?", { ev.msg.guild_id });
		bot.log(dpp::ll_debug, std::to_string(premium_filters.size()) + " premium filters to check");
		for (const db::row& row : premium_filters) {
			std::string filter_type = row.at("pattern");
			bot.log(dpp::ll_debug, "check filter " + filter_type);
			double trigger_value = 0.8, found_value = 0;
			if (row.at("score").length()) {
				trigger_value = atof(row.at("score").c_str());
			}
			json value;
			try {
				json::json_pointer ptr("/" + replace_string(filter_type, ".", "/"));
				value = response[ptr];
			}
			catch (json::exception &e) {
				bot.log(dpp::ll_debug, "Missing filter in JSON " + filter_type + ": " + e.what());
				continue;
			}
			if (!value.empty()) {
				if (value.is_number_float() || value.is_number_integer()) {
					found_value = value.get<double>();
					bot.log(dpp::ll_debug, "found value " + std::to_string(found_value));
				}
			} else {
				bot.log(dpp::ll_debug, "not found value");
				found_value = 0.0;
			}
			if (found_value >= trigger_value && found_value != 0.0) {
				bot.log(dpp::ll_info, "Matched premium filter " + filter_type + " value " + std::to_string(found_value));
				auto filter_name = db::query("SELECT description FROM premium_filter_model WHERE category = ?", { filter_type });
				std::string human_readable = filter_name[0].at("description");
				delete_message_and_warn(hash, content, bot, ev, attach, human_readable, true, found_value, trigger_value);
				return true;
			}
		}

		return false;
	}

	std::string enlarge(dpp::cluster& bot, const dpp::attachment attach, std::string file_content) {
		bot.log(dpp::ll_debug, "Enlarging small image, name: " + attach.filename);
		const char* const argv[] = {"/usr/bin/convert", "-[0]", "-resize", "128x128", "png:-", nullptr};
		spawn convert(argv);
		bot.log(dpp::ll_info, fmt::format("spawned convert; pid={} image len={}", convert.get_pid(), file_content.length()));
		convert.stdin.write(file_content.data(), file_content.length());
		convert.send_eof();
		std::ostringstream stream;
		stream << convert.stdout.rdbuf();
		file_content = stream.str();
		int ret = convert.wait();
		bot.log(ret ? dpp::ll_error : dpp::ll_info, fmt::format("convert status {}", ret));
		if (ret) {
			bot.log(dpp::ll_debug, stream.str());
		}
		return file_content;
	}

	bool perform_api_scan(int pass, bool &flattened, const std::string& hash, std::string& file_content, const dpp::attachment& attach, dpp::cluster& bot, const dpp::message_create_t ev) {
		bool rv = false;
		db::resultset settings = db::query("SELECT premium_subscription FROM guild_config WHERE guild_id = ? AND calls_this_month <= calls_limit", { ev.msg.guild_id });
		/* Only images of >= 50 pixels in both dimensions and smaller than 12mb are supported by the API. Anything else we dont scan. 
		* In the event we dont have the dimensions, scan it anyway.
		*/
		if (attach.size < 1024 * 1024 * 12 && (settings.size() && settings[0].at("premium_subscription").length())) {
			if (pass > 2) {
				bot.log(dpp::ll_warning, "Tried twice, with enlarged and normal image, could not resize");
				return false;
			}
			json& irconf = config::get("ir");
			std::vector<std::string> fields = irconf["fields"];
			std::string endpoint = irconf["host"];
			db::resultset m = db::query("SELECT GROUP_CONCAT(DISTINCT model) AS selected FROM premium_filter_model WHERE category IN (SELECT pattern FROM premium_filters WHERE guild_id = ?)", {ev.msg.guild_id});
			std::string active_models = m[0].at("selected");
			std::string original_active = active_models;
			std::vector<std::string> active = dpp::utility::tokenize(active_models, ",");
			std::vector<std::string> models = active;
			json merge = json::object();

			/* Mapping of models to cache tables */
			constexpr model_mapping cache_model[4]{
				{ "wad", "wad" },
				{ "gore", "gore" },
				{ "nudity-2.0", "nudity" },
				{ "offensive", "offensive" },
			};

			/* Retrieve models from cache */
			for (const auto& detail : cache_model) {
				if (std::find(active.begin(), active.end(), detail.model) != active.end()) {
					db::resultset rs = db::query(fmt::format("SELECT * FROM api_cache_{} WHERE hash = ?", detail.table_suffix), { hash });
					if (rs.size()) {
						active.erase(std::remove(active.begin(), active.end(), detail.model), active.end());
						if (detail.model == "wad") {
							json p = json::parse(rs[0].at("api"));
							merge.emplace("weapon", p[0].get<double>());
							merge.emplace("alcohol", p[1].get<double>());
							merge.emplace("drugs", p[2].get<double>());
						} else {
							merge.emplace(std::string(detail.table_suffix), json::parse(rs[0].at("api")));
						}
					}
				}
			}
			active_models.clear();
			for (const std::string a : active) {
				active_models.append(a).append(",");
			}
			if (active_models.length()) {
				active_models = active_models.substr(0, active_models.length() - 1);
			}

			if (!original_active.empty()) {

				json answer = merge;
				if (active.empty()) {
					bot.log(dpp::ll_info, fmt::format("image {} has all models in API cache ({})", hash, original_active));
					rv = find_banned_type(hash, answer, attach, bot, ev, file_content);
				} else {
					if (!flattened) {
						file_content = image::flatten_gif(bot, attach, file_content);
						flattened = true;
					}
					/* Make API request, upload the image, don't get the API to download it.
					* This is more expensive for us in terms of bandwidth, but we are going
					* to be able to check more images more of the time this way. We already
					* have the image data in memory and can upload it straight to the API.
					* Asking the API endpoint to download it via URL risks them being rate
					* limited or blocked as they will be making many hundreds of requests
					* per minute and we will not.
					*/
					httplib::Client cli(endpoint.c_str());
					cli.enable_server_certificate_verification(false);
					httplib::MultipartFormDataItems items = {
						{ fields[4], file_content, attach.filename, "application/octet-stream" },
						{ fields[1], irconf["credentials"]["username"], "", "" },
						{ fields[2], irconf["credentials"]["password"], "", "" },
						{ fields[0], active_models, "", "" },
					};
					auto res = cli.Post(irconf["path"].get<std::string>().c_str(), items);

					if (res) {
						if (!answer.empty()) {
							json body = json::parse(res->body);
							answer = object_merge(answer, body);
						} else {
							answer = json::parse(res->body);
						}
						if (res->status < 400) {
							bot.log(dpp::ll_info, "API response ID: " + answer["request"]["id"].get<std::string>() + " with models: " + original_active);
							rv = find_banned_type(hash, answer, attach, bot, ev, file_content);

							/* Cache each model to its cache */
							for (const auto& detail : cache_model) {
								if (std::find(models.begin(), models.end(), detail.model) != models.end()) {
									std::string v;
									if (detail.model == "wad") {
										v = "[" +
											std::to_string(answer.at("weapon").get<double>()) + "," +
											std::to_string(answer.at("alcohol").get<double>()) + "," +
											std::to_string(answer.at("drugs").get<double>()) + "]";
									} else {
										v = answer.at(std::string(detail.table_suffix)).dump();
									}
									db::query(fmt::format("INSERT INTO api_cache_{} (hash, api) VALUES(?,?) ON DUPLICATE KEY UPDATE api = ?", detail.table_suffix), { hash, v, v });
								}
							}
							db::query("UPDATE guild_config SET calls_this_month = calls_this_month + 1 WHERE guild_id = ?", {ev.msg.guild_id });
							return rv;
						} else {
							if (answer.contains("error") && answer.contains("request")) {
								int code = answer["error"]["code"].get<int>();
								std::string msg = answer["error"]["message"].get<std::string>();
								if (code == 16 && msg == "Media too small, should be at least 50 pixels in height or width")  {
									file_content = enlarge(bot, attach, file_content);
									return perform_api_scan(pass + 1, flattened, hash, file_content, attach, bot, ev);
								}
								bot.log(dpp::ll_warning, "API Error: '" + std::to_string(code) + "; " + msg  + "' status: " + std::to_string(res->status) + " id: " + answer["request"]["id"].get<std::string>());
							}
						}
					} else {
						auto err = res.error();
						bot.log(dpp::ll_warning, fmt::format("API Error: {}", httplib::to_string(err)));
					}
				}
			}
		}
		return rv;
	}

	void report(dpp::cluster& bot, bool is_good, dpp::snowflake message_id, dpp::snowflake channel_id, const std::string& image_url, const std::string& model) {
		bot.request(image_url, dpp::m_get, [&bot, is_good, message_id, channel_id, image_url, model](const dpp::http_request_completion_t& result) {
			json& irconf = config::get("ir");
			if (is_good) {
				/* TODO */
				return;
			}
			std::string feedback_class;
			if (model == "nudity-2.0") {
				feedback_class = "safe";
			} else if (model == "offensive") {
				feedback_class = "not-offensive";
			} else {
				feedback_class = "safe";
			}
			std::vector<std::string> fields = irconf["fields"];
			std::string endpoint = irconf["host"];
			httplib::Client cli(endpoint.c_str());
			cli.enable_server_certificate_verification(false);
			std::string path = image_url;
			try {
				Url u(image_url);
				path = u.path();
			}
			catch (const std::exception& e) {
			}
			std::string filename = fs::path(path).filename();
			httplib::MultipartFormDataItems items = {
				{ fields[4], result.body, filename, "application/octet-stream" },
				{ fields[1], irconf["credentials"]["username"], "", "" },
				{ fields[2], irconf["credentials"]["password"], "", "" },
				{ "model", model == "nudity-2.0" || model == "offensive" ? model : "unknown", "", "" },
				{ "class", feedback_class, "", "" },
			};
			auto res = cli.Post("/1.0/feedback.json", items);
			if (res) {
				bot.log(dpp::ll_info, fmt::format("Reported message id {} on channel {} as {}", message_id, channel_id, is_good ? "good match" : "false positive"));
			}
		});
	}
}
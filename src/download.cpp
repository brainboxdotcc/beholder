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
#include <beholder/whitelist.h>
#include <beholder/proc/json_frame.h>
#include <beholder/proc/spawn.h>
#include <CxxUrl/url.hpp>
#include <fmt/format.h>

std::atomic<int> concurrent_images{0};

static bool try_acquire_image_slot()
{
	int prev = concurrent_images.fetch_add(1);

	if (prev >= max_concurrency) {
		concurrent_images.fetch_sub(1);
		return false;
	}

	return true;
}

static void release_image_slot()
{
	concurrent_images.fetch_sub(1);
}

static std::vector<std::string> get_ocr_patterns(dpp::snowflake guild_id)
{
	std::vector<std::string> patterns;
	db::resultset rows = db::query("SELECT pattern FROM guild_patterns WHERE guild_id = ? AND pattern NOT LIKE '!%'", {guild_id});

	for (const db::row& row : rows) {
		patterns.emplace_back(row.at("pattern"));
	}

	return patterns;
}

static json get_basic_nsfw_config(dpp::cluster& bot, dpp::snowflake guild_id)
{
	db::resultset settings = db::query("SELECT basic_nsfw_suggestive, basic_nsfw_porn, basic_nsfw_drawing, basic_nsfw_hentai FROM guild_config WHERE guild_id = ?", {guild_id});

	if (settings.empty()) {
		bot.log(dpp::ll_debug, "Guild " + guild_id.str() + " using unconfigured basic scan defaults");

		return {
			{"suggestive", true},
			{"porn", true},
			{"drawing", false},
			{"hentai", true}
		};
	}

	return {
		{"suggestive", settings[0].at("basic_nsfw_suggestive") == "1"},
		{"porn", settings[0].at("basic_nsfw_porn") == "1"},
		{"drawing", settings[0].at("basic_nsfw_drawing") == "1"},
		{"hentai", settings[0].at("basic_nsfw_hentai") == "1"}
	};
}

static json get_premium_config(dpp::snowflake guild_id)
{
	json premium = {
		{"enabled", false},
		{"filters", json::array()}
	};

	db::resultset settings = db::query("SELECT premium_subscription FROM guild_config WHERE guild_id = ? AND calls_this_month <= calls_limit", {guild_id});

	if (settings.empty() || settings[0].at("premium_subscription").empty()) {
		return premium;
	}

	db::resultset rows = db::query(
		"SELECT premium_filters.pattern AS category, premium_filters.score AS threshold, premium_filter_model.description, premium_filter_model.model "
		"FROM premium_filters "
		"INNER JOIN premium_filter_model ON premium_filter_model.category = premium_filters.pattern "
		"WHERE premium_filters.guild_id = ?",
		{guild_id}
	);

	if (rows.empty()) {
		return premium;
	}

	premium["enabled"] = true;

	for (const db::row& row : rows) {
		double threshold = 0.8;

		if (!row.at("threshold").empty()) {
			threshold = atof(row.at("threshold").c_str());
		}

		premium["filters"].push_back({
						     {"category", row.at("category")},
						     {"description", row.at("description")},
						     {"model", row.at("model")},
						     {"threshold", threshold}
					     });
	}

	return premium;
}

static json get_scan_cache(const std::string& hash)
{
	json cache = json::object();

	db::resultset ocr = db::query("SELECT ocr FROM scan_cache WHERE hash = ?", {hash});

	if (!ocr.empty()) {
		cache["ocr"] = ocr[0].at("ocr");
	}

	db::resultset basic = db::query("SELECT basic FROM basic_cache WHERE hash = ?", {hash});

	if (!basic.empty()) {
		try {
			cache["basic"] = json::parse(basic[0].at("basic"));
		} catch (const json::exception&) {
		}
	}

	return cache;
}

static void write_scan_cache(const std::string& hash, const json& response, dpp::snowflake guild_id)
{
	if (!response.contains("cache") || !response.at("cache").is_object()) {
		return;
	}

	const json& cache = response.at("cache");

	if (cache.contains("ocr") && cache.at("ocr").is_string()) {
		const std::string ocr = cache.at("ocr").get<std::string>();
		db::query("INSERT INTO scan_cache (hash, ocr) VALUES(?,?) ON DUPLICATE KEY UPDATE ocr = ?", {hash, ocr, ocr});
		INCREMENT_STATISTIC("cache_miss", guild_id);
	}

	if (cache.contains("basic_nsfw") && cache.at("basic_nsfw").is_object()) {
		const std::string basic = cache.at("basic_nsfw").dump();
		db::query("INSERT INTO basic_cache (hash, basic) VALUES(?,?) ON DUPLICATE KEY UPDATE basic = ?", {hash, basic, basic});
		INCREMENT_STATISTIC("cache_miss", guild_id);
	}
}

static void increment_block_stat(const json& response, dpp::snowflake guild_id)
{
	if (!response.contains("scanner") || !response.at("scanner").is_string()) {
		return;
	}

	const std::string scanner = response.at("scanner").get<std::string>();

	if (scanner == "ocr") {
		INCREMENT_STATISTIC("images_ocr", guild_id);
	} else if (scanner == "basic_nsfw") {
		INCREMENT_STATISTIC("images_nsfw", guild_id);
	} else if (scanner == "premium") {
		if (response.contains("results") && response.at("results").is_array()) {
			for (const json& result : response.at("results")) {
				if (!result.is_object() || !result.contains("scanner") || result.at("scanner") != "premium") {
					continue;
				}

				if (!result.contains("raw") || !result.at("raw").is_object()) {
					continue;
				}

				break;
			}
		}
	}
}

bool handle_scan_response(json response, std::string hash, dpp::cluster& bot, const dpp::message_create_t ev, const dpp::attachment attach)
{
	bot.log(dpp::ll_info, "Scan hash: " + hash);
	if (!response.contains("stage") || response.at("stage") != "scan") {
		bot.log(dpp::ll_warning, "tessd returned non-scan response");
		return false;
	}
	write_scan_cache(hash, response, ev.msg.guild_id);
	if (!response.contains("status") || response.at("status") != "blocked") {
		bot.log(dpp::ll_warning, "tessd status: not blocked: " + response.dump());
		return false;
	}
	const std::string text = response.contains("text") && response.at("text").is_string() ? response.at("text").get<std::string>() : "Image blocked";
	const bool premium = response.contains("premium") && response.at("premium").is_boolean() && response.at("premium").get<bool>();
	const double trigger = response.contains("trigger") && response.at("trigger").is_number() ? response.at("trigger").get<double>() : 0.0;
	const double threshold = response.contains("threshold") && response.at("threshold").is_number() ? response.at("threshold").get<double>() : 0.0;
	bot.log(dpp::ll_warning, "delete and warn; hash=" + hash);
	increment_block_stat(response, ev.msg.guild_id);
	return delete_message_and_warn(hash, "", bot, ev, attach, text, premium, trigger, threshold);
}

json make_fetch_request(const dpp::attachment& attach)
{
	json request = {
		{"action", "fetch"},
		{"url", attach.url},
		{"filename", attach.filename}
	};

	if (attach.width) {
		request["width"] = attach.width;
	}

	if (attach.height) {
		request["height"] = attach.height;
	}

	if (attach.size) {
		request["size"] = attach.size;
	}

	return request;
}

json make_continue_request(dpp::cluster& bot, dpp::snowflake guild_id, const std::string& hash)
{
	json request = {
		{"action", "continue"},
		{"ocr_patterns", get_ocr_patterns(guild_id)},
		{"basic_nsfw", get_basic_nsfw_config(bot, guild_id)},
		{"premium", get_premium_config(guild_id)},
		{"cache", get_scan_cache(hash)}
	};

	return request;
}

void download_image(const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev)
{
	std::string lower_url = attach.url;
	std::string path;
	try {
		Url u(attach.url);
		path = u.path();
	} catch (const std::exception& e) {
		bot.log(dpp::ll_info, "Not a URL: " + attach.url + ": " + std::string(e.what()));
		return;
	}

	bot.log(dpp::ll_info, "Scan image: " + attach.url);

	for (int index = 0; whitelist[index] != nullptr; ++index) {
		if (match(attach.url.c_str(), whitelist[index])) {
			bot.log(dpp::ll_info, "Image " + attach.url + " is whitelisted by " + std::string(whitelist[index]) + "; not scanning");
			return;
		}
	}

	if (attach.width * attach.height > 33554432) {
		bot.log(dpp::ll_info, "Image dimensions of " + std::to_string(attach.width) + "x" + std::to_string(attach.height) + " too large to be a screenshot");
		return;
	}

	if (!try_acquire_image_slot()) {
		bot.log(dpp::ll_info, "Too many concurrent images, skipped");
		return;
	}

	//bot.queue_work(0, [attach, ev, &bot]() mutable {
		struct slot_guard {
			~slot_guard()
			{
				release_image_slot();
			}
		} guard;

		bot.log(dpp::ll_info, "Scanning " + attach.url);

		try {
			const char* const argv[] = {"./tessd", nullptr};
			spawn tessd(argv);

			bot.log(dpp::ll_info, fmt::format("spawned tessd; pid={}", tessd.get_pid()));

			proc::write_frame(tessd.stdin, make_fetch_request(attach));

			bot.log(dpp::ll_info, fmt::format("wrote fetch"));

			json hash_response;

			if (!proc::read_frame(tessd.stdout, hash_response)) {
				bot.log(dpp::ll_warning, "tessd did not return a hash frame");
				tessd.send_eof();
				tessd.wait();
				return;
			}

			bot.log(dpp::ll_info, fmt::format("read hash response"));

			if (!hash_response.contains("stage") || hash_response.at("stage") != "hash" || !hash_response.contains("hash")) {
				bot.log(dpp::ll_warning, "tessd returned invalid hash frame: " + hash_response.dump());
				tessd.send_eof();
				tessd.wait();
				return;
			}

			const std::string hash = hash_response.at("hash").get<std::string>();

			db::resultset block_list = db::query("SELECT hash FROM block_list_items WHERE guild_id = ? AND hash = ?", {ev.msg.guild_id, hash});

			if (!block_list.empty()) {
				bot.log(dpp::ll_info, fmt::format("writing stop action"));

				proc::write_frame(tessd.stdin, {{"action", "stop"}});

				bot.log(dpp::ll_info, fmt::format("wrote stop action"));
				tessd.send_eof();
				bot.log(dpp::ll_info, fmt::format("wrote eof"));
				tessd.wait();
				bot.log(dpp::ll_info, fmt::format("waiting reap"));

				delete_message_and_warn(hash, "", bot, ev, attach, "Image is on the block list", false);
				INCREMENT_STATISTIC2("images_scanned", "images_blocked", ev.msg.guild_id);
				return;
			}

			const json continue_request = make_continue_request(bot, ev.msg.guild_id, hash);
			proc::write_frame(tessd.stdin, continue_request);
			bot.log(dpp::ll_info, fmt::format("sending eof"));
			tessd.send_eof();
			bot.log(dpp::ll_info, fmt::format("wrote eof"));

			json scan_response;

			if (!proc::read_frame(tessd.stdout, scan_response)) {
				bot.log(dpp::ll_warning, "tessd did not return a scan frame");
				tessd.wait();
				return;
			}

			bot.log(dpp::ll_info, "handle scan response");
			handle_scan_response(scan_response, hash, bot, ev, attach);
			INCREMENT_STATISTIC("images_scanned", ev.msg.guild_id);
			bot.log(dpp::ll_info, "handle scan response done");

			bot.log(dpp::ll_info, fmt::format("waiting"));
			const int ret = tessd.wait();
			bot.log(dpp::ll_info, fmt::format("done waiting"));

			bot.log(dpp::ll_warning, fmt::format("tessd exited with status {}", ret));

		} catch (const std::exception& e) {
			bot.log(dpp::ll_error, std::string("tessd scanner failed: ") + e.what());
		}
	//});
}

bool fetch_image_hash_with_tessd(const dpp::attachment& attach, dpp::cluster& bot, std::string& hash)
{
	const char* const argv[] = {"./tessd", nullptr};
	spawn tessd(argv);

	proc::write_frame(tessd.stdin, make_fetch_request(attach));

	json hash_response;

	if (!proc::read_frame(tessd.stdout, hash_response)) {
		bot.log(dpp::ll_warning, "tessd did not return a hash frame");
		tessd.send_eof();
		tessd.wait();
		return false;
	}

	if (
		!hash_response.contains("stage")
		|| hash_response.at("stage") != "hash"
		|| !hash_response.contains("hash")
		|| !hash_response.at("hash").is_string()
		) {
		bot.log(dpp::ll_warning, "tessd returned invalid hash frame: " + hash_response.dump());
		tessd.send_eof();
		tessd.wait();
		return false;
	}

	hash = hash_response.at("hash").get<std::string>();

	proc::write_frame(tessd.stdin, {
		{"action", "stop"}
	});

	tessd.send_eof();

	const int ret = tessd.wait();

	if (ret != 0) {
		bot.log(dpp::ll_warning, fmt::format("tessd exited with status {}", ret));
		return false;
	}

	return true;
}
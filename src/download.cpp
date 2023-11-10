#include <typeinfo>
#include <dpp/dpp.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <dpp/json.h>
#include <beholder/whitelist.h>
#include <CxxUrl/url.hpp>
#include <beholder/sentry.h>

std::atomic<int> concurrent_images{0};

bool check_cached_search(const std::string& content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
	std::string hash = sha256(content);
	db::resultset rs = db::query("SELECT * FROM scan_cache WHERE hash = '?'", { hash });
	bot.log(dpp::ll_debug, "Checking cache, content hash: " + hash + " file: " + attach.filename + " found: " + std::to_string(rs.size()));
	if (rs.empty()) {
		return false;
	}
	std::string ocr = rs[0].at("ocr");
	std::string api = rs[0].at("api");

	/* Check cached OCR content */
	if (ocr.length()) {
		std::vector<std::string> lines = dpp::utility::tokenize(ocr, "\n");
		bot.log(dpp::ll_debug, "Read " + std::to_string(lines.size()) + " lines of text from cache with total size " + std::to_string(ocr.length()));
		db::resultset patterns = db::query("SELECT * FROM guild_patterns WHERE guild_id = '?'", { ev.msg.guild_id.str() });
		bot.log(dpp::ll_debug, "Checking image content against " + std::to_string(patterns.size()) + " patterns...");
		for (const std::string& line : lines) {
			if (line.length() && line[0] == 0x0C) {
				/* Tesseract puts random formdeeds in the output, skip them */
				continue;
			}
			for (const db::row& pattern : patterns) {
				const std::string& p = pattern.at("pattern");
				std::string pattern_wild = "*" + p + "*";
				if (line.length() && p.length() && match(line.c_str(), pattern_wild.c_str())) {
					delete_message_and_warn(content, bot, ev, attach, p, false);
					return true;
				}
			}
		}
	}

	bool prem = false;
	/* Check cached API content if guild is premium */
	db::resultset settings = db::query("SELECT premium_subscription FROM guild_config WHERE guild_id = ?", { ev.msg.guild_id.str() });
	if (api.length() && settings.size() && !settings[0].at("premium_subscription").empty()) {
		prem = true;
		json answer;
		try {
			answer = json::parse(api);
		} catch (const std::exception& e) {
			sentry::log_catch(typeid(e).name(), e.what());
		}
		find_banned_type(answer, attach, bot, ev, content);
		return true;
	}

	if (prem) {
		/* If theyre premium and api result in cache is empty, this returns false for a new IR scan to occur */
		return !api.empty();
	} else {
		return false;
	}
}

void download_image(const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
	std::string lower_url = dpp::lowercase(attach.url);
	std::string path;
	try {
		Url u(lower_url);
		path = u.path();
	}
	catch (const std::exception& e) {
		return;
	}
	if (path.ends_with(".webp") || path.ends_with(".jpg") || path.ends_with(".jpeg") || path.ends_with(".png") || path.ends_with(".gif")) {
		bot.log(dpp::ll_info, "Download image: " + path);
		if (concurrent_images > max_concurrency) {
			bot.log(dpp::ll_info, "Too many concurrent images, skipped");
			return;
		}
		for (int index = 0; whitelist[index] != nullptr; ++index) {
			if (match(attach.url.c_str(), whitelist[index])) {
				bot.log(dpp::ll_info, "Image " + attach.url + " is whitelisted by " + std::string(whitelist[index]) + "; not scanning");
				return;
			}
		}
		/**
		 * NOTE: The width, height and size attributes given here are only valid if the image was uploaded as
		 * an attachment. If the image we are processing came from a URL these can't be filled yet, and will
		 * be checked after we have downloaded the image. Bandwidth is cheap, so this doesnt matter too much,
		 * it's just the processing cost of running OCR on a massive image we would want to prevent.
		 */
		if (attach.width * attach.height > 33554432) {
			bot.log(dpp::ll_info, "Image dimensions of " + std::to_string(attach.width) + "x" + std::to_string(attach.height) + " too large to be a screenshot");
			return;
		}
		db::resultset pattern_count = db::query("SELECT COUNT(guild_id) AS total FROM guild_patterns WHERE guild_id = '?'", { ev.msg.guild_id.str() });
		if (pattern_count.size() == 0 || atoi(pattern_count[0].at("total").c_str()) == 0) {
			bot.log(dpp::ll_info, "No patterns defined for guild " + ev.msg.guild_id.str());
			return;
		}
		std::string url_front = attach.url;
		bot.request(attach.url, dpp::m_get, [attach, ev, &bot](const dpp::http_request_completion_t& result) {
			/**
			 * Check size of downloaded file again here, because an attachment gives us the size
			 * before we try to download it, a url does not. 
			 */
			if (check_cached_search(result.body, attach, bot, ev)) {
				return;
			}
			concurrent_images++;
			std::thread hard_work(ocr::image, result.body, attach, std::ref(bot), ev);
			hard_work.detach();
		});
	}
}


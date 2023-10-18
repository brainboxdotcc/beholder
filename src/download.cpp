#include <dpp/dpp.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <dpp/json.h>

extern std::atomic<int> concurrent_images;
extern json configdocument;

void download_image(const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
	if (attach.url.find(".webp") != std::string::npos || attach.url.find(".jpg") != std::string::npos ||
	    attach.url.find(".jpeg") != std::string::npos || attach.url.find(".png") != std::string::npos ||
	    attach.url.find(".gif") != std::string::npos) {
		bot.log(dpp::ll_info, "Image: " + attach.url);
		if (concurrent_images > max_concurrency) {
			bot.log(dpp::ll_info, "Too many concurrent images, skipped");
			return;
		}
		/**
		 * NOTE: The width, height and size attributes given here are only valid if the image was uploaded as
		 * an attachment. If the image we are processing came from a URL these can't be filled yet, and will
		 * be checked after we have downloaded the image. Bandwidth is cheap, so this doesnt matter too much,
		 * it's just the processing cost of running OCR on a massive image we would want to prevent.
		 */
		if (attach.width > 4096 || attach.height > 4096) {
			bot.log(dpp::ll_info, "Image dimensions of " + std::to_string(attach.width) + "x" + std::to_string(attach.height) + " too large to be a screenshot");
			return;
		}
		if (attach.size > max_size) {
			bot.log(dpp::ll_info, "Image size of " + std::to_string(attach.size / 1024) + "KB is larger than maximum allowed scanning size");
			return;
		}
		db::resultset pattern_count = db::query("SELECT COUNT(guild_id) AS total FROM guild_patterns WHERE guild_id = '?'", { ev.msg.guild_id.str() });
		if (pattern_count.size() == 0 || atoi(pattern_count[0].at("total").c_str()) == 0) {
			bot.log(dpp::ll_info, "No patterns defined for guild " + ev.msg.guild_id.str());
			return;
		}
		bot.request(attach.url, dpp::m_get, [attach, ev, &bot](const dpp::http_request_completion_t& result) {
			/**
			 * Check size of downloaded file again here, because an attachment gives us the size
			 * before we try to download it, a url does not. 
			 */
			if (result.body.size() > max_size) {
				bot.log(dpp::ll_info, "Image size of " + std::to_string(attach.size / 1024) + "KB is larger than maximum allowed scanning size");
			}
			bot.log(dpp::ll_debug, "Downloaded image of size: " + std::to_string(result.body.length()));
			db::resultset settings = db::query("SELECT premium_subscription FROM guild_config WHERE guild_id = '?'", { ev.msg.guild_id.str() });
			if (settings.size() && settings[0].at("premium_subscription").length()) {
				std::vector<std::string> fields = configdocument["ir"]["fields"];
				std::string endpoint = configdocument["ir"]["endpoint"];
				/* Only enable models the user has in their block list, to save on resources */
				db::resultset m = db::query("SELECT GROUP_CONCAT(DISTINCT model) AS selected FROM premium_filter_model WHERE category IN (SELECT pattern FROM premium_filters WHERE guild_id = ?)", { ev.msg.guild_id.str() });
				std::string active_models = m[0].at("selected");
				std::string url = endpoint
					+ "?" + fields[0] + "=" + dpp::utility::url_encode(active_models)
					+ "&" + fields[1] + "=" + dpp::utility::url_encode(configdocument["ir"]["credentials"]["username"])
					+ "&" + fields[2] + "=" + dpp::utility::url_encode(configdocument["ir"]["credentials"]["password"])
					+ "&" + fields[3] + "=" + dpp::utility::url_encode(attach.url);
				bot.request(url, dpp::m_get,
					[attach, ev, &bot, result, url](const dpp::http_request_completion_t& premium_api) {
						if (premium_api.status == 200 && !premium_api.body.empty()) {
							json answer = json::parse(premium_api.body);
							if (find_banned_type(answer, attach, bot, ev)) {
								return;
							}

						} else {
							bot.log(dpp::ll_debug, "API Error: " + premium_api.body + " status: " + std::to_string(premium_api.status));
						}
						concurrent_images++;
						std::thread hard_work(ocr_image, result.body, attach, std::ref(bot), ev);
						hard_work.detach();
					}
				,"", "text/json", {{"Accept", "*/*"}}, "1.0");
			} else {
				concurrent_images++;
				std::thread hard_work(ocr_image, result.body, attach, std::ref(bot), ev);
				hard_work.detach();
			}
		});
	}
}


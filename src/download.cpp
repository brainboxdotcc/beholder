#include <dpp/dpp.h>
#include <yeet/yeet.h>

extern std::atomic<int> concurrent_images;

void download_image(const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
	if (attach.url.find(".webp") != std::string::npos || attach.url.find(".jpg") != std::string::npos || attach.url.find(".png") != std::string::npos || attach.url.find(".gif") != std::string::npos) {
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
		bot.request(attach.url, dpp::m_get, [attach, ev, &bot](const dpp::http_request_completion_t& result) {
			/**
			 * Check size of downloaded file again here, because an attachment gives us the size
			 * before we try to download it, a url does not. 
			 */
			if (result.body.size() > max_size) {
				bot.log(dpp::ll_info, "Image size of " + std::to_string(attach.size / 1024) + "KB is larger than maximum allowed scanning size");
			}
			bot.log(dpp::ll_debug, "Downloaded image of size: " + std::to_string(result.body.length()));
			concurrent_images++;
			std::thread hard_work(ocr_image, result.body, attach, std::ref(bot), ev);
			hard_work.detach();
		});
	}
}


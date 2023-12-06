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
#include <beholder/config.h>
#include <beholder/image.h>
#include <beholder/sentry.h>
#include "3rdparty/httplib.h"
#include <CxxUrl/url.hpp>

std::atomic<int> concurrent_images{0};

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
		std::string url_front = attach.url;

		std::thread([attach, ev, &bot]() {
			try {
				Url u(attach.url);
				std::string path = u.path();
				std::string host = u.host();
				std::string scheme = u.scheme();
				host = scheme + "://" + host;
				httplib::Client cli(host.c_str());
				cli.enable_server_certificate_verification(false);
				cli.set_interface(config::get("tunnel_interface"));
				auto res = cli.Get(u.path());
				if (res) {
					if (res->status < 400) {
						concurrent_images++;
						std::thread hard_work(image::worker_thread, res->body, attach, std::ref(bot), ev);
						hard_work.detach();
					} else {
						bot.log(dpp::ll_warning, "Unable to fetch image: " + std::to_string(res->status)+ " - " + attach.url);	
					}
				} else {
					bot.log(dpp::ll_warning, httplib::to_string(res.error()));
				}
			}
			catch (const std::exception &e) {
				bot.log(dpp::ll_warning, e.what());
			}

		}).detach();
	}
}

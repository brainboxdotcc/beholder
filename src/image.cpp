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
#include <typeinfo>
#include <dpp/dpp.h>
#include <beholder/config.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <fmt/format.h>
#include <beholder/proc/cpipe.h>
#include <beholder/proc/spawn.h>
#include <beholder/tessd.h>
#include <beholder/sentry.h>
#include <beholder/ocr.h>
#include <beholder/premium_api.h>

namespace image {

	/**
	 * @brief Given an image file, check if it is a gif, and if it is animated.
	 * If it is, flatten it by extracting just the first frame using imagemagick.
	 * 
	 * @param bot Reference to D++ cluster
	 * @param attach message attachment
	 * @param file_content file content
	 * @return std::string new file content
	 */
	std::string flatten_gif(dpp::cluster& bot, const dpp::attachment attach, std::string file_content) {
		if (!attach.filename.ends_with(".gif") && !attach.filename.ends_with(".GIF")) {
			return file_content;
		}
		/* Animated gifs require a control structure only available in GIF89a, GIF87a is fine and anything that is
		 * neither is not a GIF file.
		 * By the way, it's pronounced GIF, as in GOLF, not JIF, as in JUMP! 🤣
		 */
		uint8_t* filebits = reinterpret_cast<uint8_t*>(file_content.data());
		if (file_content.length() >= 6 && filebits[0] == 'G' && filebits[1] == 'I' && filebits[2] == 'F' && filebits[3] == '8' && filebits[4] == '9' && filebits[5] == 'a') {
			/* If control structure is found, sequence 21 F9 04, we dont pass the gif to the API as it is likely animated
			 * This is a much faster, more lightweight check than using a GIF library.
			 */
			for (size_t x = 0; x < file_content.length() - 3; ++x) {
				if (filebits[x] == 0x21 && filebits[x + 1] == 0xF9 && filebits[x + 2] == 0x04) {
					bot.log(dpp::ll_debug, "Detected animated gif, name: " + attach.filename + "; flattening with convert");
					const char* const argv[] = {"/usr/bin/convert", "-[0]", "png:-", nullptr};
					spawn convert(argv);
					bot.log(dpp::ll_info, fmt::format("spawned convert; pid={}", convert.get_pid()));
					convert.stdin.write(file_content.data(), file_content.length());
					convert.send_eof();
					std::ostringstream stream;
					stream << convert.stdout.rdbuf();
					file_content = stream.str();
					int ret = convert.wait();
					bot.log(ret ? dpp::ll_error : dpp::ll_info, fmt::format("convert status {}", ret));
					return file_content;
				}
			}
		}
		return file_content;
	}

	void worker_thread(std::string file_content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
		dpp::utility::set_thread_name("img-scan/" + std::to_string(concurrent_images));

		on_thread_exit([&bot]() {
			bot.log(dpp::ll_info, "Scanning thread completed");
			concurrent_images--;
		});

		bool flattened = false;
		std::string hash = sha256(file_content);

		db::resultset block_list = db::query("SELECT hash FROM block_list_items WHERE guild_id = ? AND hash = ?", { ev.msg.guild_id, hash });
		if (!block_list.empty()) {
			delete_message_and_warn(hash, file_content, bot, ev, attach, "Image is on the block list", false);
			INCREMENT_STATISTIC2("images_scanned", "images_blocked", ev.msg.guild_id);
			return;
		}

		if (!ocr::scan(flattened, hash, file_content, attach, bot, ev)) {
			premium_api::perform_api_scan(1, flattened, hash, file_content, attach, bot, ev);
		}
		INCREMENT_STATISTIC("images_scanned", ev.msg.guild_id);
	}
}

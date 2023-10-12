#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <iostream>

#include <yeet/yeet.h>

using json = dpp::json;


int main(int argc, char const *argv[])
{

	/* Setup the bot */
        json configdocument;
        std::ifstream configfile("../config.json");
        configfile >> configdocument;
        dpp::cluster bot(configdocument["token"], dpp::i_default_intents | dpp::i_message_content, 1, 0, 1, true, dpp::cache_policy::cpol_none);
	dpp::snowflake home_server = std::stoull(configdocument["homeserver"].get<std::string>());

	dpp::commandhandler command_handler(&bot);
	command_handler.add_prefix(".").add_prefix("/");

	bot.on_ready([&](const dpp::ready_t &event) {
	});

	bot.on_message_create([&](const dpp::message_create_t &ev) {
		if (ev.msg.attachments.size() > 0) {
			bot.log(dpp::ll_info, "Message with images");
			for (const dpp::attachment& attach : ev.msg.attachments) {
				if (attach.url.find(".jpg") != std::string::npos || attach.url.find(".png") != std::string::npos || attach.url.find(".gif") != std::string::npos) {
					bot.log(dpp::ll_info, "Image: " + attach.url);
					dpp::utility::exec("../ocr.sh", {attach.filename, attach.url}, [attach, ev, &bot](std::string output) {
						std::vector<std::string> lines = dpp::utility::tokenize(output);
						for (const std::string& line : lines) {
							bot.log(dpp::ll_info, "Image content: " + line);
						}
					});
				}
			}
		}
	});

	bot.on_log(dpp::utility::cout_logger());

	bot.set_websocket_protocol(dpp::ws_etf);

	/* Start bot */
	bot.start(dpp::st_wait);
}

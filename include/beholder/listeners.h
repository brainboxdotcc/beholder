#pragma once
#include <dpp/dpp.h>

namespace command_listener {
	void on_slashcommand(const dpp::slashcommand_t& event);
};

namespace message_listener {
	void on_message_create(const dpp::message_create_t& event);
};

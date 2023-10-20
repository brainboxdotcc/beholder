#pragma once
#include <dpp/dpp.h>

namespace listeners {
	void on_ready(const dpp::ready_t &event);
	void on_slashcommand(const dpp::slashcommand_t& event);
	void on_message_create(const dpp::message_create_t& event);
};

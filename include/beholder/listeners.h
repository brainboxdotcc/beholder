#pragma once
#include <dpp/dpp.h>

namespace command_listener {

void on_slashcommand(const dpp::slashcommand_t& event);

};

namespace form_listener {

void on_form_submit(const dpp::form_submit_t& event);

};

namespace select_listener {

void on_select_click(const dpp::select_click_t& event);

};

namespace message_listener {

void on_message_create(const dpp::message_create_t& event);

};



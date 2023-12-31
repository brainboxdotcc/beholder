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
#include <mutex>
#include <deque>
#include <chrono>
#include <dpp/dpp.h>
#include <beholder/beholder.h>
#include <beholder/sentry.h>
#include <beholder/config.h>
#include <fmt/format.h>
#include <CxxUrl/url.hpp>
#include <sentry.h>
#include "3rdparty/httplib.h"

namespace sentry {

	sentry_transport_t *transport = nullptr;
	void *transport_state = nullptr;
	dpp::cluster* bot = nullptr;
	std::deque<std::string> send_queue;
	std::mutex send_mutex;
	std::mutex sentry_mutex;
	std::thread* work_thread = nullptr;
	std::atomic<bool> terminating = false;

	void do_sentry_send();

	/**
	 * @brief Send one envelope to sentry endpoint.
	 * These are batched up and sent in a separate thread.
	 * @param packet Envelope JSON to send to sentry
	 */
	void add_send_queue(const std::string& packet) {
		std::lock_guard<std::mutex> lock(send_mutex);
		send_queue.push_back(packet);
	}

	/**
	 * @brief Fetch all pending envelopes for sending,
	 * emptying the queue.
	 * @return A queue of pending envelopes
	 * @note The queue is emptied
	 */
	std::deque<std::string> drain_send_queue() {
		std::lock_guard<std::mutex> lock(send_mutex);
		std::deque<std::string> copy = send_queue;
		send_queue.clear();
		return copy;
	}

	size_t queue_length() {
		std::lock_guard<std::mutex> lock(send_mutex);
		return send_queue.size();
	}

	/**
	 * @brief Loop until terminated, calling the
	 * send function every minute. Bails early if terminated.
	 */
	void sentry_send_thread() {
		dpp::utility::set_thread_name("sentry_sender");
		while (!terminating) {
			/* Sleep 1 minute or until terminated */
			for (int i = 0; i < 600; ++i) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				if (terminating) {
					break;
				}
			}
			if (!terminating) {
				do_sentry_send();
			}
		}
	}

	/**
	 * @brief Attempt to send whatever is in the queue
	 * to sentry as a serial set of HTTPS POST requests
	 */
	void do_sentry_send() {
		std::deque<std::string> queue = drain_send_queue();
		for(const std::string& send_envelope : queue) {
			if (send_envelope.empty()) {
				continue;
			}
			Url dsn(config::get("sentry_dsn"));
			httplib::Client cli(dsn.scheme() + "://" + dsn.host());
			cli.enable_server_certificate_verification(false);
			cli.set_interface(config::get("safe_interface"));
			auto res = cli.Post("/api" + dsn.path() + "/envelope/?sentry_key=" + dsn.user_info() + "&sentry_version=7&sentry_client=sentry.native/7.77.0", send_envelope, "application/json");
			if (res) {
				/* Handle rate limits if headers provided */
				if (!res->get_header_value("X-Sentry-Rate-Limit-Remaining").empty() && !res->get_header_value("X-Sentry-Rate-Limit-Reset").empty()) {
					int remaining = atoi(res->get_header_value("X-Sentry-Rate-Limit-Remaining").c_str());
					time_t next = atoll(res->get_header_value("X-Sentry-Rate-Limit-Reset").c_str()) - time(nullptr);
					if (remaining == 0) {
						bot->log(dpp::ll_debug, "Sentry rate limit reached, delivery thread sleeping " + std::to_string(next) + " seconds...");
						std::this_thread::sleep_for(std::chrono::seconds(next));
					}
				}
				if (res->status >= 400) {
					bot->log(dpp::ll_debug, "Sentry post error: '" + res->body + "' status: " + std::to_string(res->status) + " event envelope: " + send_envelope);
				}
			} else {
				auto err = res.error();
				bot->log(dpp::ll_debug, "Sentry post error: " + httplib::to_string(err));
			}
		}
	}

	/**
	 * @brief Custom sentry transport, queues for delivery by cpphttplib
	 * on a separate thread.
	 */
	void custom_transport(sentry_envelope_t *envelope, void *state) {
		size_t size_out{0};
		char* env_str = sentry_envelope_serialize(envelope, &size_out);
		add_send_queue(env_str);
		sentry_free(env_str);
		sentry_envelope_free(envelope);
	}

	bool init(dpp::cluster& creator) {
		bot = &creator;
		
		std::string dsn = config::get("sentry_dsn");
		std::string env = config::get("environment");
		double sample_rate = config::get("sentry_sample_rate").get<double>();
		sentry_options_t *options = sentry_options_new();
		sentry_options_set_dsn(options, dsn.c_str());
		sentry_options_set_database_path(options, ".sentry-native");
		sentry_options_set_release(options, BEHOLDER_VERSION);
		sentry_options_set_debug(options, 0);
		sentry_options_set_environment(options, env.c_str());
		sentry_options_set_traces_sample_rate(options, sample_rate);
		transport = sentry_transport_new(custom_transport);
		sentry_transport_set_state(transport, transport_state);
		sentry_options_set_transport(options, transport);

		sentry_init(options);

		sentry_set_tag("cluster", "0");

		work_thread = new std::thread(sentry_send_thread);

		return true;
	}

	void* register_transaction_type(const std::string& transaction_name, const std::string& transaction_operation) {
		std::lock_guard<std::mutex> lock(sentry_mutex);
		return sentry_transaction_context_new(
    			transaction_name.c_str(),
    			transaction_operation.c_str()
		);
	}

	void* span(void* tx, const std::string& query) {
		std::lock_guard<std::mutex> lock(sentry_mutex);
		return sentry_transaction_start_child((sentry_transaction_t*)tx, "db.sql.query", query.c_str());
	}

	void set_span_status(void* tx, sentry_status s) {
		std::lock_guard<std::mutex> lock(sentry_mutex);
		sentry_span_t* span = (sentry_span_t*)tx;
		sentry_span_status_t status = (sentry_span_status_t)s;
		sentry_span_set_status(span, status);
	}

	void log_catch(const std::string& exception_type, const std::string& what) {
		std::lock_guard<std::mutex> lock(sentry_mutex);
		sentry_value_t exc = sentry_value_new_exception(exception_type.c_str(), what.c_str());
		sentry_value_set_stacktrace(exc, NULL, 0);
		sentry_value_t event = sentry_value_new_event();
		sentry_event_add_exception(event, exc);
		sentry_capture_event(event);
	}

	void set_user(const dpp::user& user, uint64_t guild_id) {
		std::lock_guard<std::mutex> lock(sentry_mutex);
		sentry_value_t u = sentry_value_new_object();
		sentry_value_set_by_key(u, "id", sentry_value_new_string(user.id.str().c_str()));
		sentry_value_set_by_key(u, "username", sentry_value_new_string(user.format_username().c_str()));
		sentry_value_set_by_key(u, "guild_id", sentry_value_new_string(std::to_string(guild_id).c_str()));
		sentry_set_user(u);
	}

	void unset_user() {
		std::lock_guard<std::mutex> lock(sentry_mutex);
		sentry_remove_user();
	}

	void log_message_event(const std::string& logger, const std::string& message, sentry_level_t level) {
		std::lock_guard<std::mutex> lock(sentry_mutex);
		sentry_value_t event = sentry_value_new_message_event(level, logger.c_str(), message.c_str());
		sentry_event_value_add_stacktrace(event, NULL, 0);
		sentry_capture_event(event);
	}

	void log_error(const std::string& logger, const std::string& message) {
		log_message_event(logger.c_str(), message.c_str(), SENTRY_LEVEL_ERROR);
	}

	void log_warning(const std::string& logger, const std::string& message) {
		log_message_event(logger.c_str(), message.c_str(), SENTRY_LEVEL_WARNING);
	}

	void log_critical(const std::string& logger, const std::string& message) {
		log_message_event(logger.c_str(), message.c_str(), SENTRY_LEVEL_FATAL);
	}

	void end_span(void* span) {
		std::lock_guard<std::mutex> lock(sentry_mutex);
		sentry_span_finish((sentry_span_t*)span);
	}

	std::string version() {
		return sentry_sdk_version();
	}

	void* start_transaction(void* tx_ctx) {
		if (!tx_ctx) {
			return nullptr;
		}
		std::lock_guard<std::mutex> lock(sentry_mutex);
		sentry_transaction_t* tx = sentry_transaction_start((sentry_transaction_context_t *)tx_ctx, sentry_value_new_null());
		return tx;
	}

	void end_transaction(void* opaque_transaction) {
		if (opaque_transaction) {
			std::lock_guard<std::mutex> lock(sentry_mutex);
			sentry_transaction_finish((sentry_transaction_t*)opaque_transaction);
		}
	}

	void close() {
		terminating = true;
		work_thread->join();
		do_sentry_send();
		sentry_close();
		delete work_thread;
	}
};

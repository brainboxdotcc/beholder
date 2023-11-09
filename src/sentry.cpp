#include <dpp/dpp.h>
#include <beholder/beholder.h>
#include <beholder/sentry.h>
#include <beholder/config.h>
#include <fmt/format.h>
#include <CxxUrl/url.hpp>
#include <sentry.h>
#include "3rdparty/httplib.h"

namespace sentry {

	void *transport_state = nullptr;
	dpp::cluster* bot = nullptr;

	void sentry_send(std::string send_envelope) {
		Url dsn(config::get("sentry_dsn"));
		httplib::Client cli(dsn.scheme() + "://" + dsn.host());
		cli.enable_server_certificate_verification(false);
		auto res = cli.Post("/api" + dsn.path() + "/envelope/?sentry_key=" + dsn.user_info() + "&sentry_version=7&sentry_client=sentry.native/7.77.0", send_envelope, "application/json");
		if (res) {
			if (res->status >= 400) {
				bot->log(dpp::ll_warning, "Sentry post error: '" + res->body + "' status: " + std::to_string(res->status));
			}
		} else {
			auto err = res.error();
			bot->log(dpp::ll_warning, "Sentry post error: " + httplib::to_string(err));
		}
	}

	void custom_transport(sentry_envelope_t *envelope, void *state) {
		size_t size_out{0};
		char* env_str = sentry_envelope_serialize(envelope, &size_out);
		std::string send_envelope(env_str, size_out);
		sentry_string_free(env_str);
		std::thread work(sentry_send, send_envelope);
		work.detach();
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
		sentry_transport_t *transport = sentry_transport_new(custom_transport);
		sentry_transport_set_state(transport, transport_state);
		sentry_options_set_transport(options, transport);

		return !sentry_init(options);
	}

	void* register_transaction_type(const std::string& transaction_name, const std::string& transaction_operation) {
		return sentry_transaction_context_new(
    			transaction_name.c_str(),
    			transaction_operation.c_str()
		);
	}

	void* span(void* tx, const std::string& query) {
		return sentry_transaction_start_child((sentry_transaction_t*)tx, "db.sql.query", query.c_str());
	}

	void register_error(const std::string& error) {

	}

	void end_span(void* span) {
		sentry_span_finish((sentry_span_t*)span);
	}

	std::string version() {
		return sentry_sdk_version();
	}

	void* start_transaction(void* tx_ctx) {
		if (!tx_ctx) {
			return nullptr;
		}
		sentry_transaction_t* tx = sentry_transaction_start((sentry_transaction_context_t *)tx_ctx, sentry_value_new_null());
		return tx;
	}

	void end_transaction(void* opaque_transaction) {
		if (opaque_transaction) {
			sentry_transaction_finish((sentry_transaction_t*)opaque_transaction);
		}
	}

	void close() {
		sentry_close();
	}
};

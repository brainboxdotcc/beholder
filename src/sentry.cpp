#include <beholder/beholder.h>
#include <beholder/sentry.h>
#include <beholder/config.h>
#include <fmt/format.h>
#include <sentry.h>

namespace sentry {
	bool init() {
		std::string dsn = fmt::format("https://{}.ingest.sentry.io/{}", config::get("sentry_subdomain"), config::get("sentry_id"));
		sentry_options_t *options = sentry_options_new();
		sentry_options_set_dsn(options, dsn.c_str());
		sentry_options_set_database_path(options, ".sentry-native");
		sentry_options_set_release(options, "beholder@1.0.0");
		sentry_options_set_debug(options, 0);
		sentry_options_set_environment(options, "development");
		sentry_options_set_traces_sample_rate(options, 0.2);

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

	void end_span(void* span) {
		sentry_span_finish((sentry_span_t*)span);
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
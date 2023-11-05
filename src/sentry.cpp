#include <beholder/beholder.h>
#include <beholder/sentry.h>
#include <beholder/config.h>
#include <fmt/format.h>
#include <sentry.h>

namespace sentry {
	void init() {
		std::string dsn = fmt::format("https://{}.ingest.sentry.io/{}", config::get("sentry_subdomain"), config::get("sentry_id"));
		sentry_options_t *options = sentry_options_new();
		sentry_options_set_dsn(options, dsn.c_str());
		sentry_options_set_database_path(options, ".sentry-native");
		sentry_options_set_release(options, "beholder@1.0.0");
		sentry_options_set_debug(options, 0);
		sentry_options_set_environment(options, "development");
		sentry_options_set_traces_sample_rate(options, 0.2);
		sentry_init(options);
	}

	void close() {
		sentry_close();
	}
};
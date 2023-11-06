#pragma once

namespace sentry {
	/**
	 * @brief Initialise sentry.io
	 * Expects there to be a sentry.io project of type 'native'
	 * @return true on success
	 */
	bool init();

	/**
	 * @brief Closes the sentry.io connection
	 * Flushes any events that need to be sent
	 */
	void close();

	void* register_transaction_type(const std::string& transaction_name, const std::string& transaction_operation);

	void* start_transaction(void* tx_ctx);

	void end_transaction(void* opaque_transaction);

	void* span(void* tx, const std::string& query);

	void end_span(void* span);

	std::string version();

};
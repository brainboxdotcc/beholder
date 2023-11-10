#pragma once

namespace sentry {

	enum sentry_status {
		STATUS_OK,
		STATUS_CANCELLED,
		STATUS_UNKNOWN,
		STATUS_INVALID_ARGUMENT,
		STATUS_DEADLINE_EXCEEDED,
		STATUS_NOT_FOUND,
		STATUS_ALREADY_EXISTS,
		STATUS_PERMISSION_DENIED,
		STATUS_RESOURCE_EXHAUSTED,
		STATUS_FAILED_PRECONDITION,
		STATUS_ABORTED,
		STATUS_OUT_OF_RANGE,
		STATUS_UNIMPLEMENTED,
		STATUS_INTERNAL_ERROR,
		STATUS_UNAVAILABLE,
		STATUS_DATA_LOSS,
		STATUS_UNAUTHENTICATED,
	};

	/**
	 * @brief Initialise sentry.io
	 * Expects there to be a sentry.io project of type 'native'
	 *
	 * @param creator Creating dpp cluster
	 * @return true on success
	 * 
	 * @note Reads configuration file to find the DSN and the
	 * environment name to use.
	 */
	bool init(dpp::cluster& creator);

	/**
	 * @brief Closes the sentry.io connection
	 * Flushes any events that need to be sent.
	 * 
	 * @note We don't generally reach this function because it is
	 * under a `bot.start(dpp::st_wait)`. Sentry Native flushes
	 * its queue of events every few minutes automatically.
	 */
	void close();

	/**
	 * @brief Register a new type of transaction. A transaction can optionally contain
	 * spans, and spans record types of activity with timeframes against them.
	 * 
	 * @param transaction_name Transaction name
	 * @param transaction_operation Transaction operation type
	 * @return void* Opaque pointer to sentry type
	 * 
	 * @warning These are not reusable and when assigned to a transaction, the transaction
	 * takes ownership and frees the transaction type. This is not clear in the docs at the
	 * time of writing this code!
	 */
	void* register_transaction_type(const std::string& transaction_name, const std::string& transaction_operation);

	/**
	 * @brief Start a new transaction
	 * 
	 * @param tx_ctx Transaction context from register_transaction_type()
	 * @return void* Pointer to transaction type
	 * 
	 * @warning Frees the transaction type passed in. Transaction types are NON REUSABLE and can
	 * be associated with only one transaction!
	 */
	void* start_transaction(void* tx_ctx);

	/**
	 * @brief End the transaction.
	 * 
	 * @param opaque_transaction Transaction from start_transaction
	 * 
	 * @warning Does NOT free attached spans! You must also call end_span before end_transaction
	 * if you created a span with the span() function!
	 */
	void end_transaction(void* opaque_transaction);
	
	/**
	 * @brief Create a new span within the transaction
	 * 
	 * @param tx Transaction from start_transaction()
	 * @param query Query to log against the span, e.g. an SQL query
	 * @return void* Opaque pointer to sentry span
	 */
	void* span(void* tx, const std::string& query);
	
	/**
	 * @brief Set the span status
	 * 
	 * @param tx span opaque pointer
	 * @param s span status
	 */
	void set_span_status(void* tx, sentry_status s);

	/**
	 * @brief Log a handled exception
	 * 
	 * @param exception_type exception type name
	 * @param what what() line from exception
	 */
	void log_catch(const std::string& exception_type, const std::string& what);

	/**
	 * @brief Log a non-exception error message
	 * 
	 * @param logger logger
	 * @param message error message
	 */
	void log_error(const std::string& logger, const std::string& message);

	/**
	 * @brief Log a non-exception warning message
	 * 
	 * @param logger logger
	 * @param message warning message
	 */
	void log_warning(const std::string& logger, const std::string& message);

	/**
	 * @brief Log a non-exception critical message
	 * 
	 * @param logger logger
	 * @param message critical message
	 */
	void log_critical(const std::string& logger, const std::string& message);

	/**
	 * @brief End a span. Must be called before end_transaction().
	 * 
	 * @param span Span to end
	 */
	void end_span(void* span);

	/**
	 * @brief Returns the version string for the sentry native API.
	 * 
	 * @return std::string semver version number
	 */
	std::string version();

	/**
	 * @brief Current size of log queue
	 * 
	 * @return size_t log queue length
	 */
	size_t queue_length();

};

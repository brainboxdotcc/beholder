#pragma once

namespace sentry {
	/**
	 * @brief Initialise sentry.io
	 * Expects there to be a sentry.io project of type 'native'
	 * @return true on success
	 * 
	 * @note Reads configuration file to find the DSN and the
	 * environment name to use.
	 */
	bool init();

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

};
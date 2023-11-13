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
#pragma once
#include <vector>
#include <map>
#include <string>
#include <variant>

/**
 * @brief Database abstraction layer
 * Abstracts mysql C connector, supporting prepared statements and caching.
 */
namespace db {

	/**
	 * @brief Definition of a row in a result set
	 */
	using row = std::map<std::string, std::string>;

	/**
	 * @brief Definition of a result set, a vector of maps
	 */
	using resultset = std::vector<row>;

	/**
	 * @brief A list of database query parameters.
	 * These will be translated into prepared statement arguments.
	 */
	using paramlist = std::vector<std::variant<float, std::string, uint64_t, int64_t, bool, int32_t, uint32_t, double>>;

	/**
	 * @brief Initialise database connection
	 * 
	 * @param bot creating D++ cluster
	 */
	void init (dpp::cluster& bot);

	/**
	 * @brief Connect to database and set options
	 * 
	 * @param host Database hostname
	 * @param user Database username
	 * @param pass Database password
	 * @param db Database schema name
	 * @param port Databae port number
	 * @return True if the database connection succeeded
	 */
	bool connect(const std::string &host, const std::string &user, const std::string &pass, const std::string &db, int port = 3306);

	/**
	 * @brief Disconnect from database and free query cache
	 * 
	 * @return true on successful disconnection
	 */
	bool close();

	/**
	 * @brief Run a mysql query, with automatic escaping of parameters to prevent SQL injection.
	 * 
	 * The parameters given should be a vector of strings. You can instantiate this using "{}".
	 * The queries are cached as prepared statements and therefore do not need quote symbols
	 * to be placed around parameters in the query. These will be automatically added if required.
	 * 
	 * For example:
	 * 
	 * ```cpp
	 * 	db::query("UPDATE foo SET bar = ? WHERE id = ?", { "baz", 3 });
	 * ```
	 * 
	 * Returns a resultset of the results as rows. Avoid returning massive resultsets if you can.
	 */
	resultset query(const std::string &format, const paramlist &parameters = {});

	/**
	 * @brief Returns number of affected rows from an UPDATE, INSERT, DELETE
	 * 
	 * @note This value is by any db::query() call. Take a copy!
	 * @return size_t Number of affected rows
	 */
	size_t affected_rows();

	/**
	 * @brief Returns the last error string.
	 * 
	 * @note This value is by any db::query() call. Take a copy!
	 * @return const std::string& Error mesage
	 */
	const std::string& error();

	/**
	 * @brief Returns the size of the query cache
	 * 
	 * Prepared statement handles are stored in a std::map along with their metadata, so that
	 * they don't have to be re-prepared if they are executed repeatedly. This is a diagnostic
	 * and informational function which returns the size of that map.
	 * 
	 * @return size_t Cache size
	 */
	size_t cache_size();

	/**
	 * @brief Returns total number of queries executed since connection was established
	 * 
	 * @return size_t Query counter
	 */
	size_t query_count();

	/**
	 * @brief Start a transaction
	 * 
	 * @return true if transaction was started
	 */
	bool transaction();

	/**
	 * @brief Commit a previously started transaction
	 * 
	 * @return true if transaction was committed
	 */
	bool commit();

	/**
	 * @brief Roll back a previously started transaction
	 * 
	 * @return true if transaction was rolled back
	 */
	bool rollback();

};

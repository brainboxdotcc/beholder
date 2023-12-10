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

/**
 * @brief RAII representation of a pair of pipe fds
 */
class cpipe {
private:
	/**
	 * @brief File descriptors
	 */
	int fd[2];
public:
	/**
	 * @brief Get the read side of the pipe fds
	 * 
	 * @return const int 
	 */
	int read_fd() const;

	/**
	 * @brief Get the write side of the pipe fds
	 * 
	 * @return const int 
	 */
	int write_fd() const;

	/**
	 * @brief Construct a new cpipe object
	 */
	cpipe();

	/**
	 * @brief Close both file desciptors of the pipe
	 */
	void close();

	/**
	 * @brief Destroy the cpipe object
	 */
	~cpipe();
};
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
#include <ext/stdio_filebuf.h>
#include <beholder/proc/cpipe.h>
#include <memory>

/**
 * @brief Allows two way communication with a child process via stdin/stdout
 */
class spawn {
private:
	/**
	 * @brief Child stdin pipe
	 */
	cpipe write_pipe;
	/**
	 * @brief Child stdout pipe
	 */
	cpipe read_pipe;

	/**
	 * @brief Required by wait()
	 * Captures the SIGCHLD signal, which if we do not
	 * have a handler for, we cannot receive the exit code for
	 * the child process.
	 * 
	 * @param sig 
	 */
	static void sig_chld(int sig);

public:
	pid_t child_pid{-1};
	std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > write_buf{nullptr};
	std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > read_buf{nullptr};
	std::ostream stdin;
	std::istream stdout;

	/**
	 * @brief Construct a new spawn object
	 * 
	 * @note Does not search PATH.
	 * @param argv Name and arguments to pass to child program
	 * @param envp environment to pass to child program
	 */
	spawn(const char* const argv[], const char* const envp[] = 0);

	/**
	 * @brief Send EOF to stdin on child program
	 */
	void send_eof();

	/**
	 * @brief Get the child process PID
	 * 
	 * @return pid_t child PID, or -1 if no child process running
	 */
	pid_t get_pid() const;
  
	/**
	 * @brief Wait and reap child process
	 * 
	 * @return int return code of child program
	 */
	int wait();
};


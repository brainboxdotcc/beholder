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
#include <sys/wait.h>
#include <beholder/proc/spawn.h>
#include <iostream>
#include <stdexcept>
#include <string.h>
#include <beholder/tessd.h>

namespace {

	[[noreturn]] void throw_libc_error(int error)
	{
		/* glibc uses 1024 for perror, so this should be fine */
		char errbuff[1024];
		throw std::runtime_error(strerror_r(errno, reinterpret_cast<char*>(&errbuff), sizeof(errbuff)));
	}

	/**
	 * @brief Replace the current process, executing PATH with arguments ARGV and
   	 * environment ENVP.  ARGV and ENVP are terminated by NULL pointers.
	 * 
	 * @param argv command line
	 * @param envp environment. If null, no environment is passed to the child process
	 * @return int 0 on success, -1 on failure
	 */
	int spawn_exec(char *const argv[], char *const envp[] = nullptr) {
		int result{-1};
		if (envp) {
			result = execve(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
		} else {
			result = execv(argv[0], const_cast<char* const*>(argv));
		}
		if (result == -1) {
			throw_libc_error(errno);
		}
		return result;
	}
}

void spawn::sig_chld(int sig)
{
	signal(SIGCHLD, spawn::sig_chld);
}

spawn::spawn(const char* const argv[], const char* const envp[]): stdin(nullptr), stdout(nullptr) {
	spawn::sig_chld(SIGCHLD);
	child_pid = fork();
	if (child_pid == -1) {
		throw_libc_error(errno);
	}
	if (child_pid == 0) {
		/* In child process */
		dup2(write_pipe.read_fd(), STDIN_FILENO);
		dup2(read_pipe.write_fd(), STDOUT_FILENO);
		write_pipe.close();
		read_pipe.close();
		int result{-1};
		try {
			result = spawn_exec(const_cast<char* const*>(argv), const_cast<char* const*>(envp));
		}
		catch (const std::runtime_error& e) {
			/* Note: no point writing to stdout here, it has been redirected */
			tessd::status(tessd::exit_code::exec_fail);
		}
	} else {
		close(write_pipe.read_fd());
		close(read_pipe.write_fd());
		write_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char> >(new __gnu_cxx::stdio_filebuf<char>(write_pipe.write_fd(), std::ios::out|std::ios::binary));
		read_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char> >(new __gnu_cxx::stdio_filebuf<char>(read_pipe.read_fd(), std::ios::in|std::ios::binary));
		stdin.rdbuf(write_buf.get());
		stdout.rdbuf(read_buf.get());
	}
}

void spawn::send_eof() {
	write_buf->close();
}

pid_t spawn::get_pid() const {
	return child_pid;
}

int spawn::wait() {
	int status{0};
	if (waitpid(child_pid, &status, 0) != -1) {
		return status;
	} else {
		return static_cast<int>(tessd::exit_code::waitpid);
	}
}


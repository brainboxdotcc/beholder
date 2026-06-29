/************************************************************************************
 *
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023,2026 Craig Edwards <support@sporks.gg>
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
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <beholder/proc/spawn.h>
#include <iostream>
#include <stdexcept>
#include <string.h>
#include <beholder/tessd.h>

extern char **environ;

namespace {

	[[noreturn]] void throw_libc_error(int error)
	{
		char errbuff[1024];
		throw std::runtime_error(strerror_r(error, reinterpret_cast<char*>(&errbuff), sizeof(errbuff)));
	}
}

void spawn::sig_chld(int sig)
{
	signal(SIGCHLD, spawn::sig_chld);
}

spawn::spawn(const char* const argv[], const char* const envp[]): stdin(nullptr), stdout(nullptr)
{
	spawn::sig_chld(SIGCHLD);

	posix_spawn_file_actions_t actions;

	int result = posix_spawn_file_actions_init(&actions);

	if (result != 0) {
		throw_libc_error(result);
	}

	result = posix_spawn_file_actions_adddup2(&actions, write_pipe.read_fd(), STDIN_FILENO);

	if (result != 0) {
		posix_spawn_file_actions_destroy(&actions);
		throw_libc_error(result);
	}

	result = posix_spawn_file_actions_adddup2(&actions, read_pipe.write_fd(), STDOUT_FILENO);

	if (result != 0) {
		posix_spawn_file_actions_destroy(&actions);
		throw_libc_error(result);
	}

	result = posix_spawn_file_actions_addclose(&actions, write_pipe.read_fd());

	if (result != 0) {
		posix_spawn_file_actions_destroy(&actions);
		throw_libc_error(result);
	}

	result = posix_spawn_file_actions_addclose(&actions, write_pipe.write_fd());

	if (result != 0) {
		posix_spawn_file_actions_destroy(&actions);
		throw_libc_error(result);
	}

	result = posix_spawn_file_actions_addclose(&actions, read_pipe.read_fd());

	if (result != 0) {
		posix_spawn_file_actions_destroy(&actions);
		throw_libc_error(result);
	}

	result = posix_spawn_file_actions_addclose(&actions, read_pipe.write_fd());

	if (result != 0) {
		posix_spawn_file_actions_destroy(&actions);
		throw_libc_error(result);
	}

	char* const* child_envp = envp ? const_cast<char* const*>(envp) : environ;

	result = posix_spawn(&child_pid, argv[0], &actions, nullptr, const_cast<char* const*>(argv), child_envp);

	posix_spawn_file_actions_destroy(&actions);

	if (result != 0) {
		throw_libc_error(result);
	}

	close(write_pipe.read_fd());
	close(read_pipe.write_fd());

	write_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char> >(new __gnu_cxx::stdio_filebuf<char>(write_pipe.write_fd(), std::ios::out | std::ios::binary));
	read_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char> >(new __gnu_cxx::stdio_filebuf<char>(read_pipe.read_fd(), std::ios::in | std::ios::binary));

	stdin.rdbuf(write_buf.get());
	stdout.rdbuf(read_buf.get());
}

void spawn::send_eof()
{
	write_buf->close();
}

pid_t spawn::get_pid() const
{
	return child_pid;
}

int spawn::wait()
{
	int status{0};

	if (waitpid(child_pid, &status, 0) != -1) {
		return status;
	}

	return static_cast<int>(tessd::exit_code::waitpid);
}
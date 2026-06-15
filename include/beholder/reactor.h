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
#include <dpp/dpp.h>
#include <functional>

using scan_callback = std::function<void(const std::string& hash, const dpp::json& response)>;

enum class reactor_fd_type {
	child_stdin,
	child_stdout,
	child_pid
};

enum class scan_stage {
	writing_fetch,
	waiting_hash,
	writing_continue,
	waiting_scan,
	writing_stop,
	waiting_exit
};

struct scan_request {
	dpp::attachment attach;
	dpp::message_create_t ev;
	dpp::cluster* bot;
	scan_callback callback;

	scan_request(const dpp::attachment& attach, const dpp::message_create_t& ev, dpp::cluster& bot, scan_callback callback = nullptr);
};

struct scan_job {
	dpp::attachment attach;
	dpp::message_create_t ev;
	dpp::cluster* bot;
	scan_callback callback;

	pid_t pid{-1};
	int stdin_fd{-1};
	int stdout_fd{-1};
	int pid_fd{-1};

	scan_stage stage{scan_stage::writing_fetch};

	std::string hash;
	std::string input_buffer;
	std::string output_buffer;
	size_t output_offset{0};

	bool result_received{false};

	scan_job(const scan_request& request);
};

struct reactor_fd {
	reactor_fd_type type;
	std::shared_ptr<scan_job> job;
};

class scanner_reactor {
public:
	static scanner_reactor& instance()
	{
		static scanner_reactor reactor;
		return reactor;
	}

	void submit(const dpp::attachment& attach, dpp::cluster& bot, const dpp::message_create_t& ev, scan_callback callback = nullptr);

private:
	int epoll_fd{-1};
	int queue_fd{-1};
	std::thread worker;
	std::mutex queue_mutex;
	std::deque<scan_request> requests;
	std::map<int, reactor_fd> fds;

	scanner_reactor();
	void add_fd(int fd, uint32_t events, reactor_fd_type type, const std::shared_ptr<scan_job>& job);
	void modify_fd(int fd, uint32_t events);
	void remove_fd(int& fd);
	void disable_fd(int fd);
	void run();
	void drain_queue_fd();
	void start_queued_jobs();
	void start_job(const scan_request& request);
	void handle_child_stdin(const std::shared_ptr<scan_job>& job);
	void handle_child_stdout(const std::shared_ptr<scan_job>& job);
	void process_input_lines(const std::shared_ptr<scan_job>& job);
	void process_frame(const std::shared_ptr<scan_job>& job, const json& frame);
	void process_hash_frame(const std::shared_ptr<scan_job>& job, const json& frame);
	void process_scan_frame(const std::shared_ptr<scan_job>& job, const json& frame);
	void modify_or_add_stdin(const std::shared_ptr<scan_job>& job);
	void handle_child_exit(const std::shared_ptr<scan_job>& job);
	void close_job_io(const std::shared_ptr<scan_job>& job);
};

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
#include <dpp/dpp.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include <beholder/whitelist.h>
#include <beholder/proc/json_frame.h>
#include <CxxUrl/url.hpp>
#include <fmt/core.h>
#include <beholder/reactor.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <spawn.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <beholder/listeners.h>

extern char **environ;

static std::vector<std::string> get_ocr_patterns(dpp::snowflake guild_id, dpp::snowflake channel_id)
{
	std::vector<std::string> patterns;
	db::resultset rows = db::query("SELECT pattern FROM guild_patterns WHERE guild_id = ? AND (channel_id = ? OR channel_id IS NULL)", {guild_id, channel_id});

	for (const db::row& row : rows) {
		patterns.emplace_back(row.at("pattern"));
	}

	return patterns;
}

static json get_basic_nsfw_config(dpp::cluster& bot, dpp::snowflake guild_id)
{
	db::resultset settings = db::query("SELECT basic_nsfw_suggestive, basic_nsfw_porn, basic_nsfw_drawing, basic_nsfw_hentai FROM guild_config WHERE guild_id = ?", {guild_id});

	if (settings.empty()) {
		bot.log(dpp::ll_debug, "Guild " + guild_id.str() + " using unconfigured basic scan defaults");

		return {
			{"suggestive", true},
			{"porn", true},
			{"drawing", false},
			{"hentai", true}
		};
	}

	return {
		{"suggestive", settings[0].at("basic_nsfw_suggestive") == "1"},
		{"porn", settings[0].at("basic_nsfw_porn") == "1"},
		{"drawing", settings[0].at("basic_nsfw_drawing") == "1"},
		{"hentai", settings[0].at("basic_nsfw_hentai") == "1"}
	};
}

static json get_scan_cache(const std::string& hash)
{
	json cache = json::object();

	db::resultset ocr = db::query("SELECT ocr FROM scan_cache WHERE hash = ?", {hash});

	if (!ocr.empty()) {
		cache["ocr"] = ocr[0].at("ocr");
	}

	db::resultset basic = db::query("SELECT basic FROM basic_cache WHERE hash = ?", {hash});

	if (!basic.empty()) {
		try {
			cache["basic"] = json::parse(basic[0].at("basic"));
		} catch (const json::exception&) {
		}
	}

	return cache;
}

static void write_scan_cache(const std::string& hash, const json& response, dpp::snowflake guild_id)
{
	if (!response.contains("cache") || !response.at("cache").is_object()) {
		return;
	}

	const json& cache = response.at("cache");

	if (cache.contains("ocr") && cache.at("ocr").is_string()) {
		const std::string ocr = cache.at("ocr").get<std::string>();
		db::query("INSERT INTO scan_cache (hash, ocr) VALUES(?,?) ON DUPLICATE KEY UPDATE ocr = ?", {hash, ocr, ocr});
		INCREMENT_STATISTIC("cache_miss", guild_id);
	}

	if (cache.contains("basic_nsfw") && cache.at("basic_nsfw").is_object()) {
		const std::string basic = cache.at("basic_nsfw").dump();
		db::query("INSERT INTO basic_cache (hash, basic) VALUES(?,?) ON DUPLICATE KEY UPDATE basic = ?", {hash, basic, basic});
		INCREMENT_STATISTIC("cache_miss", guild_id);
	}
}

static void increment_block_stat(const json& response, dpp::snowflake guild_id)
{
	if (!response.contains("scanner") || !response.at("scanner").is_string()) {
		return;
	}

	const std::string scanner = response.at("scanner").get<std::string>();

	if (scanner == "ocr") {
		INCREMENT_STATISTIC("images_ocr", guild_id);
	} else if (scanner == "basic_nsfw") {
		INCREMENT_STATISTIC("images_nsfw", guild_id);
	}
}

bool handle_scan_response(json response, std::string hash, dpp::cluster& bot, const dpp::message_create_t ev, const dpp::attachment attach)
{
	bot.log(dpp::ll_info, "Scan hash: " + hash);
	if (!response.contains("stage") || response.at("stage") != "scan") {
		bot.log(dpp::ll_warning, "tessd returned non-scan response");
		return false;
	}
	write_scan_cache(hash, response, ev.msg.guild_id);
	if (!response.contains("status") || response.at("status") != "blocked") {
		bot.log(dpp::ll_warning, "tessd status: not blocked: " + response.dump());
		return false;
	}
	const std::string text = response.contains("text") && response.at("text").is_string() ? response.at("text").get<std::string>() : "Image blocked";
	bot.log(dpp::ll_warning, "delete and warn; hash=" + hash);
	increment_block_stat(response, ev.msg.guild_id);
	return delete_message_and_warn(hash, "", bot, ev, attach, text);
}

json make_fetch_request(const dpp::attachment& attach)
{
	json request = {
		{"action", "fetch"},
		{"url", attach.url},
		{"filename", attach.filename}
	};

	if (attach.width) {
		request["width"] = attach.width;
	}

	if (attach.height) {
		request["height"] = attach.height;
	}

	if (attach.size) {
		request["size"] = attach.size;
	}

	return request;
}

json make_continue_request(dpp::cluster& bot, dpp::snowflake guild_id, dpp::snowflake channel_id, const std::string& hash)
{
	json request = {
		{"action", "continue"},
		{"ocr_patterns", get_ocr_patterns(guild_id, channel_id)},
		{"basic_nsfw", get_basic_nsfw_config(bot, guild_id)},
		{"cache", get_scan_cache(hash)}
	};

	return request;
}

scan_request::scan_request(const dpp::attachment& attach, const dpp::message_create_t& ev, dpp::cluster& bot, scan_callback callback) : attach(attach), ev(ev), bot(&bot), callback(callback)
{
}

scan_job::scan_job(const scan_request& request) : attach(request.attach), ev(request.ev), bot(request.bot), callback(request.callback)
{
}

int pidfd_open(pid_t pid)
{
	return static_cast<int>(syscall(SYS_pidfd_open, pid, 0));
}

void close_fd(int& fd)
{
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}

void set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		return;
	}
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

std::string make_json_frame(const json& frame)
{
	return std::string(proc::json_marker) + frame.dump() + "\n";
}

void scanner_reactor::submit(const dpp::attachment& attach, dpp::cluster& bot, const dpp::message_create_t& ev, scan_callback callback)
{
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		requests.emplace_back(attach, ev, bot, callback);
	}

	uint64_t value = 1;
	write(queue_fd, &value, sizeof(value));
}

scanner_reactor::scanner_reactor()
{
	epoll_fd = epoll_create1(EPOLL_CLOEXEC);

	if (epoll_fd == -1) {
		throw std::runtime_error("epoll_create1 failed");
	}

	queue_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

	if (queue_fd == -1) {
		throw std::runtime_error("eventfd failed");
	}

	epoll_event ev{};
	ev.events = EPOLLIN;
	ev.data.fd = queue_fd;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, queue_fd, &ev) == -1) {
		throw std::runtime_error("epoll_ctl queue_fd failed");
	}

	worker = std::thread([this]() {
		dpp::utility::set_thread_name("scan-reactor");
		run();
	});
	worker.detach();
}

void scanner_reactor::add_fd(int fd, uint32_t events, reactor_fd_type type, const std::shared_ptr<scan_job>& job)
{
	epoll_event ev{};
	ev.events = events;
	ev.data.fd = fd;

	fds[fd] = {
		.type = type,
		.job = job
	};

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		fds.erase(fd);
		throw std::runtime_error("epoll_ctl add failed");
	}
}

void scanner_reactor::modify_fd(int fd, uint32_t events)
{
	epoll_event ev{};
	ev.events = events;
	ev.data.fd = fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void scanner_reactor::remove_fd(int& fd)
{
	if (fd != -1) {
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
		fds.erase(fd);
		close_fd(fd);
	}
}

void scanner_reactor::disable_fd(int fd)
{
	if (fd != -1) {
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
		fds.erase(fd);
	}
}

void scanner_reactor::run()
{
	epoll_event events[32];

	while (true) {
		const int count = epoll_wait(epoll_fd, events, 32, -1);

		if (count == -1) {
			if (errno == EINTR) {
				continue;
			}

			continue;
		}

		for (int index = 0; index < count; ++index) {
			const int fd = events[index].data.fd;

			if (fd == queue_fd) {
				drain_queue_fd();
				start_queued_jobs();
				continue;
			}

			auto found = fds.find(fd);

			if (found == fds.end()) {
				continue;
			}

			const reactor_fd binding = found->second;

			if (binding.type == reactor_fd_type::child_stdin) {
				handle_child_stdin(binding.job);
			} else if (binding.type == reactor_fd_type::child_stdout) {
				handle_child_stdout(binding.job);
			} else if (binding.type == reactor_fd_type::child_pid) {
				handle_child_exit(binding.job);
			}
		}
	}
}

void scanner_reactor::drain_queue_fd()
{
	uint64_t value{0};

	while (read(queue_fd, &value, sizeof(value)) == sizeof(value)) {
	}
}

void scanner_reactor::start_queued_jobs()
{
	while (true) {
		std::unique_ptr<scan_request> request;

		{
			std::lock_guard<std::mutex> lock(queue_mutex);

			if (requests.empty()) {
				return;
			}

			request = std::make_unique<scan_request>(requests.front());
			requests.pop_front();
		}

		start_job(*request);
	}
}

void scanner_reactor::start_job(const scan_request& request)
{
	std::shared_ptr<scan_job> job = std::make_shared<scan_job>(request);
	job->attach = request.attach;
	job->ev = request.ev;
	job->bot = request.bot;

	int child_stdin[2]{-1, -1};
	int child_stdout[2]{-1, -1};

	if (pipe2(child_stdin, O_CLOEXEC) == -1) {
		job->bot->log(dpp::ll_error, "pipe2 child_stdin failed");
		return;
	}

	if (pipe2(child_stdout, O_CLOEXEC) == -1) {
		close(child_stdin[0]);
		close(child_stdin[1]);
		job->bot->log(dpp::ll_error, "pipe2 child_stdout failed");
		return;
	}

	posix_spawn_file_actions_t actions;
	int result = posix_spawn_file_actions_init(&actions);

	if (result != 0) {
		close(child_stdin[0]);
		close(child_stdin[1]);
		close(child_stdout[0]);
		close(child_stdout[1]);
		job->bot->log(dpp::ll_error, "posix_spawn_file_actions_init failed");
		return;
	}

	posix_spawn_file_actions_adddup2(&actions, child_stdin[0], STDIN_FILENO);
	posix_spawn_file_actions_adddup2(&actions, child_stdout[1], STDOUT_FILENO);
	posix_spawn_file_actions_addclose(&actions, child_stdin[0]);
	posix_spawn_file_actions_addclose(&actions, child_stdin[1]);
	posix_spawn_file_actions_addclose(&actions, child_stdout[0]);
	posix_spawn_file_actions_addclose(&actions, child_stdout[1]);

	const char* const argv[] = {"./tessd", nullptr};

	result = posix_spawn(
		&job->pid,
		argv[0],
		&actions,
		nullptr,
		const_cast<char* const*>(argv),
		environ
	);

	posix_spawn_file_actions_destroy(&actions);

	close(child_stdin[0]);
	close(child_stdout[1]);

	if (result != 0) {
		close(child_stdin[1]);
		close(child_stdout[0]);
		job->bot->log(dpp::ll_error, "posix_spawn failed");
		return;
	}

	job->pid_fd = pidfd_open(job->pid);

	if (job->pid_fd == -1) {
		close(child_stdin[1]);
		close(child_stdout[0]);
		kill(job->pid, SIGKILL);
		job->bot->log(dpp::ll_error, "pidfd_open failed");
		return;
	}

	job->stdin_fd = child_stdin[1];
	job->stdout_fd = child_stdout[0];

	set_nonblocking(job->stdin_fd);
	set_nonblocking(job->stdout_fd);
	set_nonblocking(job->pid_fd);

	job->stage = scan_stage::writing_fetch;
	job->output_buffer = make_json_frame(make_fetch_request(job->attach));
	job->output_offset = 0;

	job->bot->log(dpp::ll_info, fmt::format("spawned tessd; pid={}", job->pid));

	try {
		add_fd(job->stdin_fd, EPOLLOUT | EPOLLERR | EPOLLHUP, reactor_fd_type::child_stdin, job);
		add_fd(job->stdout_fd, EPOLLIN | EPOLLERR | EPOLLHUP, reactor_fd_type::child_stdout, job);
		add_fd(job->pid_fd, EPOLLIN | EPOLLERR | EPOLLHUP, reactor_fd_type::child_pid, job);
	} catch (const std::exception& e) {
		job->bot->log(dpp::ll_error, std::string("failed to register tessd fds: ") + e.what());
		close_job_io(job);
	}
}

void scanner_reactor::handle_child_stdin(const std::shared_ptr<scan_job>& job)
{
	while (job->output_offset < job->output_buffer.size()) {
		const ssize_t written = write(
			job->stdin_fd,
			job->output_buffer.data() + job->output_offset,
			job->output_buffer.size() - job->output_offset
		);

		if (written > 0) {
			job->output_offset += written;
			continue;
		}

		if (written == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			return;
		}

		job->bot->log(dpp::ll_warning, "failed writing to tessd stdin");
		close_job_io(job);
		return;
	}

	job->output_buffer.clear();
	job->output_offset = 0;

	if (job->stage == scan_stage::writing_fetch) {
		job->stage = scan_stage::waiting_hash;
		disable_fd(job->stdin_fd);
		return;
	}

	if (job->stage == scan_stage::writing_continue) {
		job->stage = scan_stage::waiting_scan;
		remove_fd(job->stdin_fd);
		return;
	}

	if (job->stage == scan_stage::writing_stop) {
		job->stage = scan_stage::waiting_exit;
		remove_fd(job->stdin_fd);
		return;
	}
}

void scanner_reactor::handle_child_stdout(const std::shared_ptr<scan_job>& job)
{
	char buffer[8192];

	while (true) {
		const ssize_t bytes = read(job->stdout_fd, buffer, sizeof(buffer));

		if (bytes > 0) {
			job->input_buffer.append(buffer, bytes);
			process_input_lines(job);
			continue;
		}

		if (bytes == 0) {
			remove_fd(job->stdout_fd);
			return;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return;
		}

		job->bot->log(dpp::ll_warning, "failed reading from tessd stdout");
		remove_fd(job->stdout_fd);
		return;
	}
}

void scanner_reactor::process_input_lines(const std::shared_ptr<scan_job>& job)
{
	while (true) {
		const size_t newline = job->input_buffer.find('\n');

		if (newline == std::string::npos) {
			return;
		}

		std::string line = job->input_buffer.substr(0, newline);
		job->input_buffer.erase(0, newline + 1);

		const size_t marker = line.find(proc::json_marker);

		if (marker != std::string::npos) {
			line = line.substr(marker + proc::json_marker.length());
		}

		json frame;

		try {
			frame = json::parse(line);
		} catch (const json::exception&) {
			continue;
		}

		process_frame(job, frame);
	}
}

void scanner_reactor::process_frame(const std::shared_ptr<scan_job>& job, const json& frame)
{
	if (job->stage == scan_stage::waiting_hash) {
		process_hash_frame(job, frame);
		return;
	}

	if (job->stage == scan_stage::waiting_scan) {
		process_scan_frame(job, frame);
		return;
	}
}

void scanner_reactor::process_hash_frame(const std::shared_ptr<scan_job>& job, const json& frame)
{
	if (!frame.contains("stage") || frame.at("stage") != "hash" || !frame.contains("hash") || !frame.at("hash").is_string()) {
		job->bot->log(dpp::ll_warning, "tessd returned invalid hash frame: " + frame.dump());
		close_job_io(job);
		return;
	}

	job->hash = frame.at("hash").get<std::string>();
	job->bot->log(dpp::ll_info, "read hash response");

	db::resultset block_list = db::query("SELECT hash FROM block_list_items WHERE guild_id = ? AND hash = ?", {job->ev.msg.guild_id, job->hash});

	if (!block_list.empty()) {
		json response = {
			{"stage", "scan"},
			{"status", "blocked"},
			{"hash", job->hash},
			{"scanner", "block_list"},
			{"scanner_name", "Admin Block List"},
			{"text", "Image is on the block list"},
			{"trigger", 1.0},
			{"threshold", 1.0},
			{"results", json::array({
				{
					{"scanner", "block_list"},
					{"scanner_name", "Admin Block List"},
					{"enabled", true},
					{"blocked", true},
					{"text", "Image is on the block list"},
					{"trigger", 1.0},
					{"threshold", 1.0},
					{"raw", json::object()}
				}
			})},
			{"cache", json::object()}
		};

		if (job->callback) {
			job->callback(job->hash, response);
		}

		job->stage = scan_stage::writing_stop;
		job->output_buffer = make_json_frame({{"action", "stop"}});
		job->output_offset = 0;
		modify_or_add_stdin(job);
		return;
	}

	job->stage = scan_stage::writing_continue;
	job->output_buffer = make_json_frame(make_continue_request(*job->bot, job->ev.msg.guild_id, job->ev.msg.channel_id, job->hash));
	job->output_offset = 0;
	modify_or_add_stdin(job);
}

void scanner_reactor::process_scan_frame(const std::shared_ptr<scan_job>& job, const json& frame)
{
	job->result_received = true;
	job->bot->log(dpp::ll_info, "handle scan response");

	if (job->callback) {
		job->callback(job->hash, frame);
	}

	INCREMENT_STATISTIC("images_scanned", job->ev.msg.guild_id);

	job->bot->log(dpp::ll_info, "handle scan response done");

	remove_fd(job->stdout_fd);
}

void scanner_reactor::modify_or_add_stdin(const std::shared_ptr<scan_job>& job)
{
	if (job->stdin_fd == -1) {
		return;
	}

	if (fds.find(job->stdin_fd) == fds.end()) {
		add_fd(job->stdin_fd, EPOLLOUT | EPOLLERR | EPOLLHUP, reactor_fd_type::child_stdin, job);
	} else {
		modify_fd(job->stdin_fd, EPOLLOUT | EPOLLERR | EPOLLHUP);
	}
}

void scanner_reactor::handle_child_exit(const std::shared_ptr<scan_job>& job)
{
	int status{0};
	waitpid(job->pid, &status, WNOHANG);

	job->bot->log(status == 0 ? dpp::ll_info : dpp::ll_warning, fmt::format("tessd exited with status {}", status));

	remove_fd(job->stdin_fd);
	remove_fd(job->stdout_fd);
	remove_fd(job->pid_fd);
}

void scanner_reactor::close_job_io(const std::shared_ptr<scan_job>& job)
{
	remove_fd(job->stdin_fd);
	remove_fd(job->stdout_fd);
}

void download_image(const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev)
{
	std::string lower_url = attach.url;
	std::string path;
	try {
		Url u(attach.url);
		path = u.path();
	} catch (const std::exception& e) {
		bot.log(dpp::ll_info, "Not a URL: " + attach.url + ": " + std::string(e.what()));
		return;
	}

	bot.log(dpp::ll_info, "Scan image: " + attach.url);

	for (int index = 0; whitelist[index] != nullptr; ++index) {
		if (match(attach.url.c_str(), whitelist[index])) {
			bot.log(dpp::ll_info, "Image " + attach.url + " is whitelisted by " + std::string(whitelist[index]) + "; not scanning");
			return;
		}
	}

	if (attach.width * attach.height > 33554432) {
		bot.log(dpp::ll_info, "Image dimensions of " + std::to_string(attach.width) + "x" + std::to_string(attach.height) + " too large to be a screenshot");
		return;
	}

	if (tessd_process_count() >= max_concurrency) {
		bot.log(dpp::ll_info, "Too many concurrent images, skipped");
		return;
	}

	scanner_reactor::instance().submit(attach, bot, ev, [&bot, ev, attach](const std::string& hash, const json& response) {
		handle_scan_response(response, hash, bot, ev, attach);
	});
}

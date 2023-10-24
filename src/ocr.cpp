#include <dpp/dpp.h>
#include <beholder/config.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include "3rdparty/httplib.h"
#include <ext/stdio_filebuf.h> // NB: Specific to libstdc++
#include <sys/wait.h>
#include <fmt/format.h>

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
	const inline int read_fd() const {
		return fd[0];
	}

	/**
	 * @brief Get the write side of the pipe fds
	 * 
	 * @return const int 
	 */
	const inline int write_fd() const {
		return fd[1];
	}

	/**
	 * @brief Construct a new cpipe object
	 */
	cpipe() {
		if (pipe(fd)) {
			throw std::runtime_error("Failed to create pipe");
		}
	}

	/**
	 * @brief Close both file desciptors of the pipe
	 */
	void close() {
		::close(fd[0]);
		::close(fd[1]);
	}

	/**
	 * @brief Destroy the cpipe object
	 */
	~cpipe() {
		close();
	}
};

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
public:
	int child_pid{-1};
	std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > write_buf{nullptr};
	std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > read_buf{nullptr};
	std::ostream stdin;
	std::istream stdout;

	/**
	 * @brief Construct a new spawn object
	 * 
	 * @param argv Name and arguments to pass to child program
	 * @param with_path inherit environment
	 * @param envp environment to pass to child program
	 */
	spawn(const char* const argv[], bool with_path = false, const char* const envp[] = 0): stdin(nullptr), stdout(nullptr) {
		child_pid = fork();
		if (child_pid == -1) {
			throw std::runtime_error("Failed to start child process"); 
		}
		if (child_pid == 0) {   // In child process
			dup2(write_pipe.read_fd(), STDIN_FILENO);
			dup2(read_pipe.write_fd(), STDOUT_FILENO);
			write_pipe.close();
			read_pipe.close();
			int result;
			if (with_path) {
				if (envp != 0) {
					result = execvpe(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
				} else {
					result = execvp(argv[0], const_cast<char* const*>(argv));
				}
			}
			else {
				if (envp != 0) {
					result = execve(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
				} else {
					result = execv(argv[0], const_cast<char* const*>(argv));
				}
			}
			if (result == -1) {
				// Note: no point writing to stdout here, it has been redirected
				std::cerr << "Error: Failed to launch program" << std::endl;
				exit(1);
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

	/**
	 * @brief Send EOF to stdin on child program
	 */
	void send_eof() {
		write_buf->close();
	}
    
	/**
	 * @brief Wait and reap child process
	 * 
	 * @return int return code of child program
	 */
	int wait() {
		int status{0};
		waitpid(child_pid, &status, 0);
		return status;
	}
};

namespace ocr {

	void image(std::string file_content, const dpp::attachment attach, dpp::cluster& bot, const dpp::message_create_t ev) {
		std::string ocr;

		dpp::utility::set_thread_name("img-scan/" + std::to_string(concurrent_images));

		on_thread_exit([&bot]() {
			bot.log(dpp::ll_info, "Scanning thread completed");
			concurrent_images--;
		});

		/* This loop is not redundant - it ensures the spawn class is scoped */
		try {
			const char* const argv[] = {"./tessd", (const char*)0};
			spawn tessd(argv);
			tessd.stdin.write(file_content.data(), file_content.length());
			tessd.send_eof();
			std::string l;
			while (std::getline(tessd.stdout, l)) {
				ocr.append(l).append("\n");
			}
			int ret = tessd.wait();
			if (ret != 0) {
				bot.log(dpp::ll_error, fmt::format("tessd returned nonzero exit code {}", ret));
			}
		} catch (const std::runtime_error& e) {
			bot.log(dpp::ll_error, e.what());
		}

		std::vector<std::string> lines = dpp::utility::tokenize(ocr, "\n");
		bot.log(dpp::ll_debug, "Read " + std::to_string(lines.size()) + " lines of text from image with total size " + std::to_string(ocr.length()));
		db::resultset patterns = db::query("SELECT * FROM guild_patterns WHERE guild_id = '?'", { ev.msg.guild_id.str() });
		bot.log(dpp::ll_debug, "Checking image content against " + std::to_string(patterns.size()) + " patterns...");
		for (const std::string& line : lines) {
			for (const db::row& pattern : patterns) {
				const std::string& p = pattern.at("pattern");
				std::string pattern_wild = "*" + p + "*";
				if (line.length() && p.length() && match(line.c_str(), pattern_wild.c_str())) {
					delete_message_and_warn(file_content, bot, ev, attach, p, false);
					std::string hash = sha256(file_content);
					db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
					return;
				}
			}
		}

		db::resultset settings = db::query("SELECT premium_subscription FROM guild_config WHERE guild_id = ? AND calls_this_month <= calls_limit", { ev.msg.guild_id.str() });
		/* Only images of >= 50 pixels in both dimension are supported by the API. Anything else we dont scan. 
		 * In the event we dont have the dimensions, scan it anyway.
		 */
		if ((attach.width == 0 || attach.width >= 50) && (attach.height == 0 || attach.height >= 50)
			&& settings.size() && settings[0].at("premium_subscription").length()) {
			/* Animated gifs require a control structure only available in GIF89a */
			if (file_content[0] == 'G' && file_content[1] == 'I' && file_content[2] == 'F' && file_content[3] == '8' && file_content[4] == '9' && file_content[5] == 'a') {
				/* If control structure is found, sequence 21 F9 04, we dont pass the gif to the API as it is likely animated
				 * This is a much faster, more lightweight check than using a GIF library.
				 */
				for (size_t x = 0; x < file_content.length() - 3; ++x) {
					if (file_content[x] == 0x21 && file_content[x + 1] == 0xF9 && file_content[x + 2] == 0x04) {
						bot.log(dpp::ll_debug, "Detected animated gif, name: " + attach.filename + "; not scanning with IR");
						std::string hash = sha256(file_content);
						db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
						return;
					}
				}
			}
			json& irconf = config::get("ir");
			std::vector<std::string> fields = irconf["fields"];
			std::string endpoint = irconf["host"];
			db::resultset m = db::query("SELECT GROUP_CONCAT(DISTINCT model) AS selected FROM premium_filter_model");
			std::string active_models = m[0].at("selected");
			std::string url = irconf["path"].get<std::string>()
				+ "?" + fields[0] + "=" + dpp::utility::url_encode(active_models)
				+ "&" + fields[1] + "=" + dpp::utility::url_encode(irconf["credentials"]["username"])
				+ "&" + fields[2] + "=" + dpp::utility::url_encode(irconf["credentials"]["password"])
				+ "&" + fields[3] + "=" + dpp::utility::url_encode(attach.url);
			db::query("UPDATE guild_config SET calls_this_month = calls_this_month + 1 WHERE guild_id = ?", {ev.msg.guild_id.str() });

			/* Build httplib client */
			bot.log(dpp::ll_debug, "Host: " + endpoint + " url: " + url);
			httplib::Client cli(endpoint.c_str());
			cli.enable_server_certificate_verification(false);
			std::string rv;
			int code = 0;
			auto res = cli.Get(url.c_str());
			if (res) {
				url = endpoint + url;
				if (res->status < 400) {
					json answer;
					try {
						answer = json::parse(res->body);
					} catch (const std::exception& e) {
					}
					find_banned_type(answer, attach, bot, ev, file_content);
					std::string hash = sha256(file_content);
					db::query("INSERT INTO scan_cache (hash, ocr, api) VALUES('?','?','?') ON DUPLICATE KEY UPDATE ocr = '?', api = '?'", { hash, ocr, res->body, ocr, res->body });
				} else {
					bot.log(dpp::ll_debug, "API Error: '" + res->body + "' status: " + std::to_string(res->status));
					std::string hash = sha256(file_content);
					db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
				}
			} else {
				bot.log(dpp::ll_debug, "API Error(2) " + res->body + " " + std::to_string(res->status));
				std::string hash = sha256(file_content);
				db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
			}
		} else {
			std::string hash = sha256(file_content);
			db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
		}
	}

}

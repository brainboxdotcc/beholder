#include <dpp/dpp.h>
#include <beholder/config.h>
#include <beholder/beholder.h>
#include <beholder/database.h>
#include "3rdparty/EasyGifReader.h"
#include "3rdparty/httplib.h"
#include <ext/stdio_filebuf.h> // NB: Specific to libstdc++
#include <sys/wait.h>

// Wrapping pipe in a class makes sure they are closed when we leave scope
class cpipe {
private:
	int fd[2];
public:
	const inline int read_fd() const {
		return fd[0];
	}

	const inline int write_fd() const {
		return fd[1];
	}

	cpipe() {
		if (pipe(fd)) throw std::runtime_error("Failed to create pipe");
	}

	void close() {
		::close(fd[0]); ::close(fd[1]);
	}

	~cpipe() {
		close();
	}
};

class spawn {
private:
	cpipe write_pipe;
	cpipe read_pipe;
public:
	int child_pid = -1;
	std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > write_buf = NULL; 
	std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > read_buf = NULL;
	std::ostream stdin;
	std::istream stdout;
    
	spawn(const char* const argv[], bool with_path = false, const char* const envp[] = 0): stdin(NULL), stdout(NULL) {
		child_pid = fork();
		if (child_pid == -1) throw std::runtime_error("Failed to start child process"); 
		if (child_pid == 0) {   // In child process
			dup2(write_pipe.read_fd(), STDIN_FILENO);
			dup2(read_pipe.write_fd(), STDOUT_FILENO);
			write_pipe.close(); read_pipe.close();
			int result;
			if (with_path) {
				if (envp != 0) result = execvpe(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
				else result = execvp(argv[0], const_cast<char* const*>(argv));
			}
			else {
				if (envp != 0) result = execve(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
				else result = execv(argv[0], const_cast<char* const*>(argv));
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
    
	void send_eof() {
		write_buf->close();
	}
    
	int wait() {
		int status;
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
		do {
			const char* const argv[] = {"./tessd", (const char*)0};
			spawn tessd(argv);
			tessd.stdin.write(file_content.data(), file_content.length());
			tessd.send_eof();
			std::string l;
			while (std::getline(tessd.stdout, l)) {
				ocr.append(l + "\n");
			}
			tessd.wait();
		} while (0);

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
		if (settings.size() && settings[0].at("premium_subscription").length()) {
			try {
				EasyGifReader gif_reader = EasyGifReader::openMemory(file_content.data(), file_content.length());
				int frame_count = gif_reader.frameCount();
				if (frame_count > 1) {
					bot.log(dpp::ll_debug, "Detected animated gif with " + std::to_string(frame_count) + " frames, name: " + attach.filename + "; not scanning with IR");
					std::string hash = sha256(file_content);
					db::query("INSERT INTO scan_cache (hash, ocr) VALUES('?','?') ON DUPLICATE KEY UPDATE ocr = '?'", { hash, ocr, ocr });
					return;
				}
			} catch (const EasyGifReader::Error& error) {
				/* Not a gif, this is not a fatal error */
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
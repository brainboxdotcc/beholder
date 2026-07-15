#include <beholder/logger.h>
#include <nsfwd/nsfwd.h>
#include <nsfwd/log_aggregator.h>
#include <fstream>
#include <signal.h>
#include <unistd.h>

constexpr size_t max_child_rss = 8ULL * 1024 * 1024 * 1024;

static size_t ticks = 0;

static size_t child_rss(pid_t pid) {
	std::ifstream statm("/proc/" + std::to_string(pid) + "/statm");
	size_t pages = 0;
	size_t resident = 0;
	statm >> pages >> resident;
	return resident * sysconf(_SC_PAGESIZE);
}

[[noreturn]] void run_supervisor(const char* self) {
	logger::init("nsfwd-logs/nsfwd.log");

	while (true) {
		const char *args[] = { self, "--child", nullptr };
		spawn child(args);
		std::string line;

		while (std::getline(child.stdout, line)) {
			log_child_line(line);
			ticks++;

			if (ticks % 10) {
				continue;
			}
			if (child_rss(child.get_pid()) > max_child_rss) {
				dpp::log_t l;
				l.severity = dpp::ll_warning;
				l.message = "nsfwd child exceeded memory limit, restarting";
				logger::log(l);

				kill(child.get_pid(), SIGTERM);
				break;
			}
		}

		child.wait();
	}
}
#include <beholder/logger.h>
#include <nsfwd/nsfwd.h>
#include <nsfwd/log_aggregator.h>

int run_supervisor(const char* self) {
	logger::init("logs/nsfwd.log");

	const char *args[] = { self, "--child", nullptr };
	spawn child(args);
	std::string line;

	while (std::getline(child.stdout, line)) {
		log_child_line(line);
	}
	return child.wait();
}


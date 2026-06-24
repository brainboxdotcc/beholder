#include <string_view>
#include <dpp/dpp.h>
#include <beholder/logger.h>
#include <nsfwd/log_aggregator.h>
#include <trantor/utils/Logger.h>

void log_child_line(std::string_view line) {
	dpp::log_t l;
	l.severity = dpp::ll_info;

	while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
		line.remove_suffix(1);
	}

	if (line.empty()) {
		return;
	}

	/* Tensorflow pre-absl */
	if (line.starts_with("WARNING: ")) {
		l.severity = dpp::ll_warning;
		l.message = std::string(line.substr(9));
		logger::log(l);
		return;
	}
		/* Trantor */
	else if (line.size() > 43 && line.find(" UTC ") != std::string_view::npos) {
		size_t level = line.find(" INFO  ");
		if (level != std::string_view::npos) {
			l.severity = dpp::ll_info;
			line.remove_prefix(level + 7);
		} else if ((level = line.find(" WARN  ")) != std::string_view::npos) {
			l.severity = dpp::ll_warning;
			line.remove_prefix(level + 7);
		} else if ((level = line.find(" ERROR ")) != std::string_view::npos) {
			l.severity = dpp::ll_error;
			line.remove_prefix(level + 7);
		} else if ((level = line.find(" FATAL ")) != std::string_view::npos) {
			l.severity = dpp::ll_critical;
			line.remove_prefix(level + 7);
		}

		size_t colon = line.rfind(':');
		if (colon != std::string_view::npos) {
			bool numeric = colon + 1 < line.size();
			for (size_t i = colon + 1; i < line.size(); ++i) {
				if (!isdigit(line[i])) {
					numeric = false;
					break;
				}
			}
			if (numeric) {
				size_t dash = line.rfind(" - ", colon);
				if (dash != std::string_view::npos) {
					line.remove_suffix(line.size() - dash);
				}
			}
		}
	}
		/* TensorFlow absl */
	else if (!line.empty() && (line[0] == 'I' || line[0] == 'W' || line[0] == 'E' || line[0] == 'F') && line.size() > 1 && isdigit(line[1])) {
		switch (line[0]) {
			case 'W':
				l.severity = dpp::ll_warning;
				break;
			case 'E':
				l.severity = dpp::ll_error;
				break;
			case 'F':
				l.severity = dpp::ll_critical;
				break;
		}
		size_t bracket = line.find("] ");
		if (bracket != std::string_view::npos) {
			line.remove_prefix(bracket + 2);
		}
	}
		/* TensorFlow classic */
	else if (line.size() > 25 && isdigit(line[0]) && isdigit(line[1])) {
		size_t level = line.find(": ");
		if (level != std::string_view::npos && level + 2 < line.size()) {
			switch (line[level + 2]) {
				case 'W':
					l.severity = dpp::ll_warning;
					break;
				case 'E':
					l.severity = dpp::ll_error;
					break;
				case 'F':
					l.severity = dpp::ll_critical;
					break;
			}
		}
		size_t bracket = line.find("] ");
		if (bracket != std::string_view::npos) {
			line.remove_prefix(bracket + 2);
		}
	}

	l.message = std::string(line);
	logger::log(l);
}

void server_log_init() {
	dup2(STDOUT_FILENO, STDERR_FILENO);
	trantor::Logger::setOutputFunction([](const char *msg, uint64_t len) {
		std::cout << std::string(msg, len) << std::endl;
	}, []() {});
}
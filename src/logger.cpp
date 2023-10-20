#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <dpp/dpp.h>

namespace logger {

	using spdlog_level = spdlog::level::level_enum;

	constexpr const char* log_file = "logs/beholder.log";
	constexpr int max_log_size = 1024 * 1024 * 5;

	static std::shared_ptr<spdlog::logger> async_logger;

	void init()
	{
		/* Set up spdlog logger */
		spdlog::init_thread_pool(8192, 2);
		std::vector<spdlog::sink_ptr> sinks = {
			std::make_shared<spdlog::sinks::stdout_color_sink_mt >(),
			std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file, max_log_size, 10)
		};
		async_logger = std::make_shared<spdlog::async_logger>("file_and_console", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		async_logger->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t]%$ : %v");
		async_logger->set_level(spdlog_level::debug);
		spdlog::register_logger(async_logger);
	}

	void log(dpp::cluster& bot)
	{
		bot.on_log([&bot](const dpp::log_t & event) {
			switch (event.severity) {
				case dpp::ll_trace: async_logger->trace("{}", event.message); break;
				case dpp::ll_debug: async_logger->debug("{}", event.message); break;
				case dpp::ll_info: async_logger->info("{}", event.message); break;
				case dpp::ll_warning: async_logger->warn("{}", event.message); break;
				case dpp::ll_error: async_logger->error("{}", event.message); break;
				case dpp::ll_critical: async_logger->critical("{}", event.message); break;
			}
		});
	}
}
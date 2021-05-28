#include "log.hpp"

#include "lib/assert.hpp"
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>

std::shared_ptr<spdlog::logger> logger = nullptr;
std::shared_ptr<spdlog::logger> logger_process = nullptr;

void initialize_logger(std::string pattern, bool load_env_levels, bool force_colors)
{
    ASSERT(logger == nullptr);
    ASSERT(logger_process == nullptr);

    // st = single-threaded, without locking
    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_st>(
      force_colors ? spdlog::color_mode::always : spdlog::color_mode::automatic);

    logger = std::make_shared<spdlog::logger>("main", stderr_sink);
    logger_process = std::make_shared<spdlog::logger>("process", stderr_sink);

    spdlog::register_logger(logger);
    spdlog::register_logger(logger_process);

    logger->set_level(spdlog::level::info);
    // info level for process contains some prints
    //  that probably aren't too useful for casual user
    logger_process->set_level(spdlog::level::warn);

    if (load_env_levels) {
        // set log level from environment, e.g., export SPDLOG_LEVEL=info
        spdlog::cfg::load_env_levels();
    }

    logger->set_pattern(std::move(pattern));
}

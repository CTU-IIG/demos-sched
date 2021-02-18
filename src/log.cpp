#include "log.hpp"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/cfg/env.h"

std::shared_ptr<spdlog::logger> logger = nullptr;

void initialize_logger(std::string pattern, bool load_env_levels, bool force_colors)
{
    assert(logger == nullptr);
    logger = spdlog::stderr_color_st(
      "stderr", force_colors ? spdlog::color_mode::always : spdlog::color_mode::automatic);

    if (load_env_levels) {
        // set loglevel from environment, e.g., export SPDLOG_LEVEL=info
        spdlog::cfg::load_env_levels();
    }

    logger->set_pattern(pattern);
}
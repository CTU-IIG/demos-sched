#ifndef LOG_HPP
#define LOG_HPP

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=const"
#include "spdlog/spdlog.h"
#pragma GCC diagnostic pop

/** A global spdlog logger instance, initialized by `initialize_logger(...)`. */
extern std::shared_ptr<spdlog::logger> logger;

/**
 * Sets up a global spdlog logger stored in the global `logger` variable.
 * @param pattern - logger format string
 * @param load_env_levels - if true, spdlog will load log level from SPDLOG_LEVEL env var
 * @param force_colors - if true, spdlog always uses color escape codes in logs
 */
void initialize_logger(std::string pattern, bool load_env_levels, bool force_colors);

#endif

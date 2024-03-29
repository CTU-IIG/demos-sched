#pragma once

// sets SPDLOG_ACTIVE_LEVEL
#include "config_meson.h"

#pragma GCC diagnostic push
// this suppresses the GCC warning about unknown pragma below
#pragma GCC diagnostic ignored "-Wpragmas"
// this suppresses the warning about unknown warning in Clang below, but it's not known for GCC
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
// this pragma is supported in GCC, but not Clang
#pragma GCC diagnostic ignored "-Wsuggest-attribute=const"
#pragma GCC diagnostic ignored "-Weffc++"
#include <spdlog/spdlog.h>
#pragma GCC diagnostic pop
// spdlog headers contain <cassert>, which we don't want,
//  as we use `ASSERT` from lib/assert.hpp instead
// undef to prevent accidental misuse
#undef assert
#undef assert_perror

/** A global spdlog logger instance, initialized by `initialize_logger(...)`. */
extern std::shared_ptr<spdlog::logger> logger;
/**
 * Logger used by Process. For a larger number of processes,
 * it is quite talkative, which is why it was separated to a new logger.
 */
extern std::shared_ptr<spdlog::logger> logger_process;

#define TRACE(...) SPDLOG_LOGGER_TRACE(logger, __VA_ARGS__)
#define TRACE_PROCESS(...) SPDLOG_LOGGER_TRACE(logger_process, __VA_ARGS__)

/**
 * Sets up a global spdlog logger stored in the global `logger` variable.
 * @param pattern - logger format string
 * @param load_env_levels - if true, spdlog will load log level from SPDLOG_LEVEL env var
 * @param force_colors - if true, spdlog always uses color escape codes in logs
 */
void initialize_logger(std::string pattern, bool load_env_levels, bool force_colors);

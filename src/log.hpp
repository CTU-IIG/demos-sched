#ifndef LOG_HPP
#define LOG_HPP

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=const"
#include "spdlog/spdlog.h"
#pragma GCC diagnostic pop

extern std::shared_ptr<spdlog::logger> logger;

#endif

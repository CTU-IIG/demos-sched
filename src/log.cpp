#include "log.hpp"

#include "spdlog/sinks/stdout_color_sinks.h"

std::shared_ptr<spdlog::logger> logger = spdlog::stderr_color_st("stderr");

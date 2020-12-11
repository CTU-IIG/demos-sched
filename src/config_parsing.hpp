#ifndef CONFIG_PARSING_HPP
#define CONFIG_PARSING_HPP

#include "demossched.hpp"
#include "majorframe.hpp"
#include <ev++.h>

typedef struct
{
    Cgroup &unified_cg;
    Cgroup &cpuset_cg;
    Cgroup &freezer_cg;
    ev::default_loop &loop;
    std::chrono::steady_clock::time_point &start_time;
} CgroupConfig;

#endif // CONFIG_PARSING_HPP

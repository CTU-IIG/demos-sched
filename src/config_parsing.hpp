#ifndef CONFIG_PARSING_HPP
#define CONFIG_PARSING_HPP

#include "check_lib.hpp"
#include "majorframe.hpp"
#include <ev++.h>

typedef struct
{
    Cgroup &unified_cg;
    Cgroup &cpuset_cg;
    Cgroup &freezer_cg;
    ev::default_loop &loop;
} CgroupConfig;

#endif // CONFIG_PARSING_HPP

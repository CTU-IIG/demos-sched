#ifndef CONFIG_PARSING_HPP
#define CONFIG_PARSING_HPP

#include "demossched.hpp"
#include "majorframe.hpp"
#include <ev++.h>
#include <yaml-cpp/yaml.h>

typedef struct
{
    Cgroup &unified_cg;
    Cgroup &cpuset_cg;
    Cgroup &freezer_cg;
    ev::default_loop &loop;
    std::chrono::steady_clock::time_point &start_time;
} CgroupConfig;

void normalize_config(YAML::Node &in_c, YAML::Node &out_c);
void create_demos_objects(const YAML::Node &config, const CgroupConfig &c, Windows &windows, Partitions &partitions);

#endif // CONFIG_PARSING_HPP

#ifndef CONFIG_PARSING_HPP
#define CONFIG_PARSING_HPP

#include "demossched.hpp"
#include "majorframe.hpp"
#include <yaml-cpp/yaml.h>
#include <ev++.h>

typedef struct{
    YAML::Node &config;
    Cgroup &unified_cg;
    Cgroup &cpuset_cg;
    Cgroup &freezer_cg;
    ev::default_loop &loop;
    std::chrono::steady_clock::time_point &start_time;
} Config;

void parse_config(Config &c, Windows &windows, Partitions &partitions);

Partition *find_partition_by_name(std::string name, Config &c, Partitions &partitions, std::chrono::milliseconds &default_budget);

#endif // CONFIG_PARSING_HPP

#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <iostream>
#include "majorframe.hpp"
#include <yaml-cpp/yaml.h>
#include <ev++.h>
#include <string.h>

class ConfigParser
{
public:
    ConfigParser(std::string config_file);

    void parse(ev::loop_ref loop, std::shared_ptr<MajorFrame> mf_ptr);

private:
    std::string freezer_path = "/sys/fs/cgroup/freezer/my_cgroup";
    std::string cpuset_path = "/sys/fs/cgroup/cpuset/my_cgroup";
    std::string unified_path = "/sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup";

    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

    YAML::Node config;
    //std::vector<std::string> partition_names;
    Partitions partitions;
    Partition empty_part;
    //std::list< Slices > all_slices;
    Windows windows;
};

#endif // CONFIG_PARSER_HPP

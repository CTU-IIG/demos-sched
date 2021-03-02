#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "check_lib.hpp"
#include "majorframe.hpp"
#include <ev++.h>
// silence warnings from yaml-cpp header files
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop

typedef struct
{
    Cgroup &unified_cg;
    Cgroup &cpuset_cg;
    Cgroup &freezer_cg;
    ev::default_loop &loop;
} CgroupConfig;

class Config
{
public:
    void load_from_file(const std::string& file_name);
    void load_from_string(const std::string& config_str);

    void normalize();
    void create_demos_objects(const CgroupConfig &c, Windows &windows, Partitions &partitions);

    const YAML::Node &get() const { return config; }

private:
    YAML::Node config{};
    int anonymous_partition_counter = 0;

    YAML::Node normalize_window(const YAML::Node &win, YAML::Node &partitions);
    YAML::Node normalize_partition(const YAML::Node &part, float total_budget);
    YAML::Node normalize_slice(const YAML::Node &slice, float win_length, YAML::Node &partitions);
    std::string process_xx_partition_and_get_name(const YAML::Node &part,
                                                  float total_budget,
                                                  YAML::Node &partitions);
    std::string process_xx_processes_and_get_name(const YAML::Node &processes,
                                                  float total_budget,
                                                  YAML::Node &partitions);
};

#endif // CONFIG_HPP

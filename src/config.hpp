#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "config_parsing.hpp" // TODO: Remove after refactoring
#include <yaml-cpp/yaml.h>

class Config
{
public:
    void loadFile(std::string file_name);
    void loadStr(std::string cfg);

    void normalize();
    void create_demos_objects(const CgroupConfig &c, Windows &windows, Partitions &partitions);

    const YAML::Node &get() const { return config; }

private:
    YAML::Node config{};
    int anonymous_partition_counter = 0;

    void parse_partitions(YAML::Node &norm_slice,
                          const YAML::Node &parent,
                          YAML::Node &out_c,
                          const YAML::Node &in_c,
                          int w_length);

    void find_partition_by_name(const std::string name,
                                YAML::Node &norm_slice,
                                const YAML::Node &parent,
                                YAML::Node &out_config,
                                const YAML::Node &in_config,
                                const std::string key,
                                int total_budget);

    static int compute_default_budget(int total_budget, int num_of_procs);

    void create_partition_from_cmds(YAML::Node &norm_slice,
                                    const YAML::Node &parent,
                                    YAML::Node &out_c,
                                    const std::string prefix,
                                    int total_budget);

    void create_partition_from_window(YAML::Node &norm_slice,
                                      const YAML::Node &parent,
                                      YAML::Node &out_c,
                                      const std::string key,
                                      int total_budget);

    YAML::Node normalize_window(const YAML::Node &win, YAML::Node &partitions);
    YAML::Node normalize_processes(const YAML::Node &processes, float total_budget);
    YAML::Node normalize_partition(const YAML::Node &part, float total_budget);
    YAML::Node normalize_process(const YAML::Node &process, float default_budget);
    YAML::Node normalize_slice(const YAML::Node &slice, float win_length, YAML::Node &partitions);
    std::string process_xx_partition_and_get_name(const YAML::Node &part,
                                                  float total_budget,
                                                  YAML::Node &partitions);
    std::string process_xx_processes_and_get_name(const YAML::Node &processes,
                                                  float total_budget,
                                                  YAML::Node &partitions);
};

#endif // CONFIG_HPP

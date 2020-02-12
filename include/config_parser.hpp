#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <iostream>
#include "majorframe.hpp"
#include <yaml-cpp/yaml.h>
#include <ev++.h>

class ConfigParser
{
public:
    ConfigParser(std::string config_file);

    void parse(ev::loop_ref loop, std::shared_ptr<MajorFrame> mf_ptr);

private:
    YAML::Node config;
    Partitions partitions;
    std::list< Slices > all_slices;
    Windows windows;
};

#endif // CONFIG_PARSER_HPP

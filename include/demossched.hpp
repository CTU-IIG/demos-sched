#ifndef DEMOSSCHED_HPP
#define DEMOSSCHED_HPP

#include <iostream>
#include <chrono>
#include <err.h>

class DemosSched
{
public:
    static void init();
    static void set_cgroup_paths(std::string freezer, std::string cpuset);

protected:
    static std::string freezer_path;
    static std::string cpuset_path;

    static std::chrono::steady_clock::time_point start_time;

    static bool initialized;
};

#endif // DEMOSSCHED_HPP

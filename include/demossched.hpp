#ifndef DEMOSSCHED_HPP
#define DEMOSSCHED_HPP

#include <iostream>
#include <chrono>
#include <err.h>

#define CHECK(expr) ({ decltype (expr) ret = (expr); if (ret == -1) throw std::system_error(errno, std::generic_category(), std::string(__PRETTY_FUNCTION__) + ": " #expr); ret; })

class DemosSched
{
public:
    static void init();
    static void set_cgroup_paths(std::string freezer, std::string cpuset);

protected:
    static std::string freezer_path;
    static std::string cpuset_path;
    static std::string unified_path;

    static std::chrono::steady_clock::time_point start_time;

    static bool initialized;
};

#endif // DEMOSSCHED_HPP

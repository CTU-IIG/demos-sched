#include "demossched.hpp"

// default static members values
std::string DemosSched::freezer_path = "/sys/fs/cgroup/freezer/my_cgroup/";
std::string DemosSched::cpuset_path = "/sys/fs/cgroup/cpuset/my_cgroup/";
std::string DemosSched::unified_path = "/sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup/";

std::chrono::steady_clock::time_point DemosSched::start_time;

bool DemosSched::initialized = false;

void DemosSched::init()
{
    if( initialized ){
        warn("no effect, init DemosSched just once");
        return;
    }
    initialized = true;

    // set start time
    start_time = std::chrono::steady_clock::now();

    // init random seed
    srand(time(NULL));

}

void DemosSched::set_cgroup_paths(std::string freezer, std::string cpuset)
{
    // set paths to cgroups
    freezer_path = freezer;
    cpuset_path = cpuset;
}

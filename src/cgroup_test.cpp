#include "cgroup.hpp"
#include "demossched.hpp"
#include "ev++.h"
#include <iostream>

using namespace std;

ev::default_loop loop;

void cb1(bool populated)
{
    // cerr<< populated <<endl;
}

void cb2(bool populated)
{
    // cerr<< populated <<endl;
    loop.break_loop(ev::ALL);
}

int main()
{

    try {
        std::string freezer_path = "/sys/fs/cgroup/freezer/my_cgroup";
        std::string cpuset_path = "/sys/fs/cgroup/cpuset/my_cgroup";
        std::string unified_path =
          "/sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/"
          "my_cgroup";

        CgroupFreezer partition_freezer(freezer_path, "partition");
        CgroupFreezer process_freezer(partition_freezer, "process");

        CgroupCpuset partition_cpuset(cpuset_path, "partition");

        CgroupEvents partition_event(loop, unified_path, "partition", cb1);
        CgroupEvents process_event(loop, partition_event, "process", cb2);

        loop.run();

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}

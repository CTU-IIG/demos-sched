#include "ev++.h"
#include "partition.hpp"
#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono_literals;

// needs refactor, API changed in the meantime
int main()
{
    ev::default_loop loop;

    try {
        std::string freezer_path = "/sys/fs/cgroup/freezer/my_cgroup";
        std::string cpuset_path = "/sys/fs/cgroup/cpuset/my_cgroup";
        std::string unified_path =
          "/sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/"
          "my_cgroup";

        Partition part(freezer_path, cpuset_path, unified_path, "partA");
        part.add_process(loop, "src/tests/infinite_proc 1000000 be2_A", 1s);
        part.get_current_proc().resume();

        loop.run();

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}

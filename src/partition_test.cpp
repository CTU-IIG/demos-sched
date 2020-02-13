#include <iostream>
#include "demossched.hpp"
#include "partition.hpp"
#include "ev++.h"
#include <chrono>

using namespace std;
using namespace std::chrono_literals;

int main()
{
    ev::default_loop loop;

    try{
        std::string freezer_path = "/sys/fs/cgroup/freezer/my_cgroup";
        std::string cpuset_path = "/sys/fs/cgroup/cpuset/my_cgroup";
        std::string unified_path = "/sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup";

        Partition part(loop, freezer_path, cpuset_path, unified_path, "partA");
        part.add_process(loop, std::vector<char*>{"src/infinite_proc","1000000","be2_A"}, 1s);
        part.unfreeze();

        loop.run();

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}

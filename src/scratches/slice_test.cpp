#include "ev++.h"
#include "slice.hpp"
#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono_literals;

int main()
{
    ev::default_loop loop;

    try {
        std::string freezer_path = "/sys/fs/cgroup/freezer/my_cgroup";
        std::string cpuset_path = "/sys/fs/cgroup/cpuset/my_cgroup";
        std::string unified_path =
          "/sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/"
          "my_cgroup";

        auto start_time = chrono::steady_clock::now();

        Partition sc_part(freezer_path, cpuset_path, unified_path, "sc_part");
        sc_part.add_process(loop, "src/infinite_proc 200000 procA", 1s);
        sc_part.add_process(loop, "src/infinite_proc 200000 procB", 1s);

        Partition be_part(freezer_path, cpuset_path, unified_path, "be_part");
        be_part.add_process(loop, "src/infinite_proc 200000 procBE", 2s);

        Slice s(loop, start_time, sc_part, be_part, "0,3-4");
        s.start();

        loop.run();

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}

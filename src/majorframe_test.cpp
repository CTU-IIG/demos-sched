#include "check_lib.hpp"
#include "ev++.h"
#include "majorframe.hpp"
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

        Partition sc_partA(freezer_path, cpuset_path, unified_path, "sc_part");
        sc_partA.add_process(loop, "src/infinite_proc 200000 procA", 1s);

        Partition sc_partB(freezer_path, cpuset_path, unified_path, "sc_part");
        sc_partB.add_process(loop, "src/infinite_proc 200000 procB", 1s);

        Partition be_part(freezer_path, cpuset_path, unified_path, "be_part");

        Slices s1;
        s1.emplace_back(loop, start_time, sc_partA, be_part, "0,3-5");

        Slices s2;
        s2.emplace_back(loop, start_time, sc_partA, be_part, "0");
        s2.emplace_back(loop, start_time, sc_partB, be_part, "1");

        Windows w;
        w.emplace_back(s1, 1s);
        w.emplace_back(s2, 1s);

        MajorFrame mf(loop, start_time, w);
        mf.start();

        loop.run();

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}

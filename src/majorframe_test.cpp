#include <iostream>
#include "demossched.hpp"
#include "majorframe.hpp"
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

        auto start_time = chrono::steady_clock::now();

        Partition sc_part(loop, freezer_path, cpuset_path, unified_path, "sc_part");
        sc_part.add_process(loop, std::vector<char*>{"src/infinite_proc","200000","procA"}, 1s);
        sc_part.add_process(loop, std::vector<char*>{"src/infinite_proc","200000","procB"}, 1s);

        Partition be_part(loop, freezer_path, cpuset_path, unified_path, "be_part");
        be_part.add_process(loop, std::vector<char*>{"src/infinite_proc","200000","procBE"}, 1s);

        Partition empty_part(loop, freezer_path, cpuset_path, unified_path, "empty_part");

        Slices s1,s2;
        s1.emplace_back(loop, start_time, sc_part, be_part);
        s2.emplace_back(loop, start_time, empty_part, be_part);

        Windows w;
        w.emplace_back( s1,2500ms );
        w.emplace_back( s2,1s );

        MajorFrame mf(loop, start_time, w);
        mf.start();

        loop.run();

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}

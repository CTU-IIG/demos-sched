#include "majorframe.hpp"
#include <list>
#include <iostream>

using namespace std::chrono_literals;

void print_help()
{
    printf("help:\n"
            "-h\n"
            "\t print this message\n"
            "-g <NAME>\n"
            "\t name of cgroup with user access,\n"
            "\t need to create /sys/fs/cgroup/freezer/<NAME> and\n"
            "\t /sys/fs/cgroup/cpuset/<NAME> manually, then\n"
            "\t sudo chown -R <user> .../<NAME>\n"
            "\t if not set, \"my_cgroup\" is used\n"
            "TODO -c <FILE>\n"
            "\t path to configuration file\n");
}

int main(int argc, char *argv[]) 
{
    // parse arguments
    int opt;
    while((opt = getopt(argc, argv, "hg:c:")) != -1){
        switch(opt){
            case 'h':
                print_help();
                exit(0);
            case 'g':{
                std::string cgrp_name = std::string(optarg);
                // set paths to cgroups
                DemosSched::set_cgroup_paths("/sys/fs/cgroup/freezer/" + cgrp_name + "/", "/sys/fs/cgroup/cpuset/" + cgrp_name + "/");
                break;}
            case 'c':
                break;
            default:
                print_help();
                exit(1);
        }
    }
    DemosSched::init();

    // parse yaml config
    // consistency check

    // forks, pipes, exec, cgroups, ...
    
    ev::default_loop loop;
    // TEST data structures
    try {

        Partition sc1(loop,"_SC1"), be1(loop,"_BE1");
        sc1.add_process( loop, "procA", std::vector<std::string>
            {"src/infinite_proc","200000","sc1_A"}, 1s );
        sc1.add_process( loop, "procB", std::vector<std::string>
            {"src/infinite_proc","200000","sc1_B"}, 1s );
        //throw "test";
        be1.add_process( loop, "procC", std::vector<std::string>
            {"src/infinite_proc","200000","be1_A"}, 1s );

        Partition sc2(loop, "_SC2"), be2(loop,"_BE2");
        be2.add_process( loop, "procD", std::vector<std::string>
            {"src/infinite_proc","200000","be2_A"}, 1s );

        Slices slices1, slices2;
        slices1.emplace_back(loop, sc1, be1, Cpu(1) );
        slices2.emplace_back(loop, sc2, be2, Cpu(1) );

        Windows windows;
        windows.push_back( Window(loop, slices1, 3s));
        windows.push_back( Window(loop, slices2, 1s));
        //throw "test";
        MajorFrame mf(loop, windows );

        //slices2.begin()->start();
        mf.start();

//        Process &proc = mf.get_current_window().slices.front().sc.get_current_proc();
//        proc.exec();
//        proc.unfreeze();

        // configure linux scheduler
        //struct sched_param sp = {.sched_priority = 99};
        //if (sched_setscheduler( 0, SCHED_FIFO, &sp ) == -1)
            //kill_procs_and_exit("sched_setscheduler");

        //throw "test";
        loop.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    //mf.kill_all();
    // Wait for all processes to terminate
    //loop.run(); // terminate imediately if there is no watchers

    // Wait for all processes to terminate
//    if (num_populated > 0)
//        loop.run();

    
    //delete proc_ptr;
    //proc_ptr->clean();
    //kill_procs_and_exit("exit");

    return 0;
}


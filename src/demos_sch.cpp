#include "classes.hpp"
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
    
    // TEST data structures
    ev::default_loop loop;
    try {
        std::list<Process> be_procs, sc_procs;
        sc_procs.emplace_back("procA", std::vector<std::string>
                    {"src/infinite_proc","1000000","hello"}, 10ms,2ms);
        sc_procs.emplace_back("procB", std::vector<std::string>
                    {"/bin/echo","foo"}, 5ms,1ms);
        be_procs.emplace_back("procC", std::vector<std::string>
                    {"/bin/echo","best effort"}, 5ms,1ms);

        Partition sc = Partition(std::move(sc_procs));
        Partition be = Partition(std::move(be_procs));

        Slice s = {.sc = sc, .be = be, .cpus = 1};
        Window w1 = {.length = 1s, .slices = std::vector<Slice> {s} };
        Window w2 = {.length = 500ms, .slices = std::vector<Slice> {s, s} };
        MajorFrame frame = {.length = 60ms, .windows = std::vector<Window> {w1, w2} };

        //std::cout<< frame.get_budgets()[0].count() <<std::endl;
        //std::cout<< frame.windows[0].slices[0].sc.get_budgets()[0].count() <<std::endl;
        Process & proc_ptr = frame.windows[0].slices[0].sc.get_current_proc();
        //proc_ptr->recompute_budget();
        //std::cout<<proc_ptr->get_actual_budget().count()<<std::endl;
        proc_ptr.exec();
        proc_ptr.start_timer(3s);
        proc_ptr.unfreeze();

        // configure linux scheduler
        //struct sched_param sp = {.sched_priority = 99};
        //if (sched_setscheduler( 0, SCHED_FIFO, &sp ) == -1)
            //kill_procs_and_exit("sched_setscheduler");
    //sys/fs/cgroup/freezer
        // how to use exception here?
        //try{
            //int ret = sched_setscheduler( 0, SCHED_FIFO, &sp );
            //std::cout<<ret<<std::endl;
        //} catch (int e){
            //kill_procs_and_exit("sched_setscheduler");
        //}

        loop.run(0);
    } catch (const char* msg) {
        std::cerr << msg << std::endl;
        loop.break_loop(ev::ALL);
    }

    
    //delete proc_ptr;
    //proc_ptr->clean();
    //kill_procs_and_exit("exit");

    return 0;
}


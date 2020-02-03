#include "classes.hpp"
#include "functions.hpp"

using namespace std::chrono_literals;

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
                Process::set_cgroup_paths("/sys/fs/cgroup/freezer/" + cgrp_name + "/", "/sys/fs/cgroup/cpuset/" + cgrp_name + "/");
                break;}
            case 'c':
                break;
            default:
                print_help();
                exit(1);
        }
    }

    // init random seed
    srand(time(NULL));

    // get start time
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    //std::cout<<start_time.time_since_epoch().count()<<std::endl;
    //std::cout<<(start_time + 10ns).time_since_epoch().count()<<std::endl; 

    // parse yaml config
    // consistency check

    // forks, pipes, exec, cgroups, ...
    
    // TEST data structures
    Process procA("procA", std::vector<std::string>
            {"./infinite_proc","1000000","hello"}, start_time, 10ms,2ms);
    Process procB("procB", std::vector<std::string>
            {"/bin/echo","foo"}, start_time, 5ms,1ms);
    Process procC("procC", std::vector<std::string>
            {"/bin/echo","best effort"}, start_time, 5ms,1ms);
    //std::cout<< procA.exec() <<std::endl;

    Partition sc = Partition( std::vector<Process> {procA, procB});
    Partition be = Partition( std::vector<Process> {procC});

    Slice s = {.sc = sc, .be = be, .cpus = 1};
    Window w1 = {.length = 1s, .slices = std::vector<Slice> {s} };
    Window w2 = {.length = 500ms, .slices = std::vector<Slice> {s, s} };
    MajorFrame frame = {.length = 60ms, .windows = std::vector<Window> {w1, w2} };

    //std::cout<< frame.get_budgets()[0].count() <<std::endl;
    //std::cout<< frame.windows[0].slices[0].sc.get_budgets()[0].count() <<std::endl;
    Process* proc_ptr = frame.windows[0].slices[0].sc.get_current_proc();
    //proc_ptr->recompute_budget();
    //std::cout<<proc_ptr->get_actual_budget().count()<<std::endl;
    proc_ptr->exec();
    proc_ptr->start_timer(3s);
    proc_ptr->unfreeze();
    
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

    ev::default_loop loop;
    loop.run(0);
    
    //delete proc_ptr;
    //proc_ptr->clean();
    //kill_procs_and_exit("exit");

    return 0;
}


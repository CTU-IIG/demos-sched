#ifndef PARTITION_HPP
#define PARTITION_HPP

#include "process.hpp"
#include "cgroup.hpp"
#include "demossched.hpp"
#include <list>
#include <chrono>

typedef std::list<Process> Processes;

class Partition : protected DemosSched
{
public:
    //Partition( Processes &&processes, std::string cgroup_name );
    Partition(ev::loop_ref loop, std::string partition_name = "" );
    ~Partition();

    Process & get_current_proc();

    void freeze();
    void unfreeze();

    // TODO somehow call Process() without copying
    void add_process(ev::loop_ref loop,
                     std::vector<char *> argv,
                     std::chrono::nanoseconds budget,
                     std::chrono::nanoseconds budget_jitter = std::chrono::nanoseconds(0));

    void set_cpus(std::string cpus);
    bool is_done();
    bool is_empty();
    std::string get_name();
    Processes processes;

    // return false if there is none
    bool move_to_next_unfinished_proc();
    void clear_done_flag();
private:
    bool done = false;
    bool empty = true;
    Processes::iterator current;
    Cgroup cgroup;
    std::string name;

    void proc_exit_cb(Process &proc);

    // cyclic queue
    void move_to_next_proc();

    // counter for created partitions to have unique cgroup names
    //static int cgrp_count;
};

#endif // PARTITION_HPP

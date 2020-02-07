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
    // cyclic queue
    void move_to_next_proc();

    // TODO somehow call Process() without copying
    void add_process(ev::loop_ref loop,
                     std::string name,
                     std::vector<std::string> argv,
                     std::chrono::nanoseconds budget,
                     std::chrono::nanoseconds budget_jitter = std::chrono::nanoseconds(0));
    static int cgrp_count;

    Processes processes;
private:
    Processes::iterator current;
    Cgroup cgroup;
    std::string cgrp_name;

    // counter for created partitions to have unique cgroup names
    //static int cgrp_count;
};

#endif // PARTITION_HPP

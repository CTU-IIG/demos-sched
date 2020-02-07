#include "partition.hpp"

int Partition::cgrp_count = 0;

//Partition::Partition(Processes &&processes, std::string cgroup_name )
//    : processes(std::move(processes))
//    , current( this->processes.begin() )
//    , cgroup(cgroup_name)
//{
//    //std::cerr << __PRETTY_FUNCTION__ << this << std::endl;
//}

Partition::Partition( ev::loop_ref loop, std::string partition_name)
    : cgroup( loop, std::to_string(cgrp_count) + partition_name )
    , cgrp_name( std::to_string(cgrp_count) + partition_name )
{
    cgrp_count++;
}

Partition::~Partition()
{
    //std::cerr<< __PRETTY_FUNCTION__ << " PID:"+std::to_string(getpid())+" @" << this << " " << cgrp_name <<std::endl;
    // remove process cgroups first
    processes.clear();
}

void Partition::add_process(ev::loop_ref loop,
                            std::string name,
                            std::vector<std::string> argv,
                            std::chrono::nanoseconds budget,
                            std::chrono::nanoseconds budget_jitter)
{
    processes.emplace_back( loop, cgrp_name + "/" + name, argv, budget, budget_jitter);
    current = processes.begin();
}

Process & Partition::get_current_proc()
{
    return *current;
}

// cyclic queue
void Partition::move_to_next_proc()
{
    if( ++current == processes.end() )
        current = processes.begin();
}

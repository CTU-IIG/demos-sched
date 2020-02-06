#include "partition.hpp"

Partition::Partition(Processes &&processes, std::string cgroup_name )
    : processes(std::move(processes))
    , current( this->processes.begin() )
    , cgroup(cgroup_name)
{
    //std::cerr << __PRETTY_FUNCTION__ << this << std::endl;
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

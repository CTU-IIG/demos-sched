#include "partition.hpp"

//Partition::Partition(Processes &&processes, std::string cgroup_name )
//    : processes(std::move(processes))
//    , current( this->processes.begin() )
//    , cgroup(cgroup_name)
//{
//    //std::cerr << __PRETTY_FUNCTION__ << this << std::endl;
//}

Partition::Partition( ev::loop_ref loop, std::string partition_name)
    : cgroup( loop, false, partition_name )
    , name(partition_name)
{
}

Partition::~Partition()
{
    //std::cerr<< __PRETTY_FUNCTION__ << " PID:"+std::to_string(getpid())+" @" << this << " " << cgrp_name <<std::endl;
    // remove process cgroups first
    processes.clear();
}

void Partition::add_process(ev::loop_ref loop,
                            std::vector<char*> argv,
                            std::chrono::nanoseconds budget,
                            std::chrono::nanoseconds budget_jitter)
{
    std::cerr<<__PRETTY_FUNCTION__<<" "<<cgroup.get_name()<<std::endl;
    processes.emplace_back( loop, cgroup.get_name(), cgroup.get_fd_cpuset_procs(),
                            argv, budget, budget_jitter);

    // call of move_to_next_proc first
    current = processes.end();
    empty = false;
    freeze();
}

void Partition::set_cpus(std::string cpus)
{
    cgroup.set_cpus(cpus);
}

bool Partition::is_done()
{
    return done;
}

void Partition::clear_done_flag()
{
    done = false;
}

bool Partition::is_empty()
{
    return empty;
}

std::string Partition::get_name()
{
    return name;
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

bool Partition::move_to_next_unfinished_proc()
{
    for(Process &p : processes) {
        move_to_next_proc();
        if( !current->is_completed() )
            return true;
    }
    done = true;
    return false;
}


void Partition::freeze()
{
    cgroup.freeze();
}

void Partition::unfreeze()
{
    cgroup.unfreeze();
}


#include "process.hpp"
#include <system_error>
#include <functional>
#include "partition.hpp"

using namespace std::placeholders;
using namespace std;

Process::Process(ev::loop_ref loop,
                 std::string name,
                 Partition &part,
                 string argv,
                 std::chrono::nanoseconds budget,
                 std::chrono::nanoseconds budget_jitter, bool contionuous)
    : part(part)
    , cge(loop, part.cge, name, std::bind(&Process::populated_cb, this, _1))
    , cgf(part.cgf, name)
    , argv(argv)
    , budget(budget)
    , budget_jitter(budget_jitter)
    , actual_budget(budget)
    , continuous(contionuous)
    //, cgroup(loop, true, argv[0], partition_cgrp_name)
    // TODO regex to cut argv[0] at "/"
    //, cgroup(loop, true, "test", fd_cpuset_procs, partition_cgrp_name)
    //, cge(loop, part.cgroup, name, std::bind(&Process::populated_cb, this, _1))
{
    //std::cerr<<__PRETTY_FUNCTION__<<" "<<name<<std::endl;
    freeze();
}

void Process::exec()
{
    //TODO pipe

    // create new process
    pid = CHECK(vfork());

    // launch new process
    if( pid == 0 ){
        // CHILD PROCESS
        CHECK(execl( "/bin/bash",  "/bin/bash", "-c" , argv.c_str(), nullptr ));
        // END CHILD PROCESS
    } else {
        // PARENT PROCESS
#ifdef VERBOSE
    std::cerr<< __PRETTY_FUNCTION__ << " " << argv << " pid: " << pid << std::endl;
#endif
        // add process to cgroup (echo PID > cgroup.procs)
        cge.add_process(pid);
        cgf.add_process(pid);
        // END PARENT PROCESS
    }
}

void Process::kill()
{
    if( !killed ){
        cgf.freeze();
        cgf.kill_all();
        cgf.unfreeze();
    }
    killed = true;
}

void Process::freeze()
{
    cgf.freeze();
}

void Process::unfreeze()
{
    cgf.unfreeze();
}

void Process::recompute_budget()
{
    std::chrono::nanoseconds rnd_val= budget_jitter * rand()/RAND_MAX;
    actual_budget = budget - budget_jitter/2 + rnd_val;
}

std::chrono::nanoseconds Process::get_actual_budget()
{
    return actual_budget;
}

bool Process::is_completed()
{
    return completed;
}

void Process::mark_completed()
{
    if( !continuous )
        completed = true;
}

void Process::mark_uncompleted()
{
    completed = false;
}

pid_t Process::get_pid()
{
    return pid;
}

void Process::populated_cb(bool populated)
{
    if(!populated){
        part.proc_exit_cb(*this);
    }
}

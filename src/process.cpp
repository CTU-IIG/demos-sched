#include "process.hpp"
#include <system_error>
#include <functional>
#include "partition.hpp"

using namespace std::placeholders;

Process::Process(ev::loop_ref loop,
                 Partition &part,
                 std::vector<char*> argv,
                 std::chrono::nanoseconds budget,
                 std::chrono::nanoseconds budget_jitter,
                 bool continuous)
    : partition_cgrp_name(partition_cgrp_name)
    , argv(argv)
    , budget(budget)
    , budget_jitter(budget_jitter)
    , actual_budget(budget)
    , continuous(continuous)
    //, cgroup(loop, true, argv[0], partition_cgrp_name)
    // TODO regex to cut argv[0] at "/"
    //, cgroup(loop, true, "test", fd_cpuset_procs, partition_cgrp_name)
    , cge(loop, part.cgroup, name, std::bind(&Process::populated_cb, this, _1))
{
    try {
        this->argv.push_back((char*) nullptr);
        exec();
    } catch (const std::exception& e) {
        delete &cgroup;
        throw e;
    }
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

void Process::populated_cb(bool populated)
{
    //...
    part.proc_exit_cb(*this);
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

void Process::kill()
{
    cgroup.kill_all();
}

void Process::exec()
{
#ifdef VERBOSE
    std::cerr<< __PRETTY_FUNCTION__ << " " << argv[0] <<std::endl;
#endif
    //TODO pipe

    // freeze cgroup
    freeze();

    // create new process
    pid = CHECK(vfork());

    // launch new process
    if( pid == 0 ){
        // CHILD PROCESS
        CHECK(execv( argv[0], &argv[0] ));
        // END CHILD PROCESS
    } else {
        // PARENT PROCESS
        // add process to cgroup (echo PID > cgroup.procs)
        cgroup.add_process(pid);
        // END PARENT PROCESS
    }
}

void Process::freeze()
{
    cgroup.freeze();
}

void Process::unfreeze()
{
    cgroup.unfreeze();
}

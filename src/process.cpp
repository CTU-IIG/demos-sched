#include "process.hpp"
#include <system_error>
#include <functional>
#include "partition.hpp"

using namespace std::placeholders;
using namespace std;

Process::Process(ev::loop_ref loop,
                 std::string name,
                 Partition &part,
                 std::vector<char*> argv,
                 std::chrono::nanoseconds budget,
                 std::chrono::nanoseconds budget_jitter)
    : part(part)
    , cge(loop, part.cge, name, std::bind(&Process::populated_cb, this, _1))
    , cgf(part.cgf, name)
    , argv(argv)
    , budget(budget)
    , budget_jitter(budget_jitter)
    , actual_budget(budget)
    //, cgroup(loop, true, argv[0], partition_cgrp_name)
    // TODO regex to cut argv[0] at "/"
    //, cgroup(loop, true, "test", fd_cpuset_procs, partition_cgrp_name)
    //, cge(loop, part.cgroup, name, std::bind(&Process::populated_cb, this, _1))
{
    std::cerr<<__PRETTY_FUNCTION__<<" "<<name<<std::endl;
    this->argv.push_back((char*) nullptr);
    freeze();
}

void Process::exec()
{
    //TODO pipe

    // create new process
    pid_t pid = CHECK(vfork());

    // launch new process
    if( pid == 0 ){
        // CHILD PROCESS
        CHECK(execv( argv[0], &argv[0] ));
        // END CHILD PROCESS
    } else {
        // PARENT PROCESS
#ifdef VERBOSE
    std::cerr<< __PRETTY_FUNCTION__ << " " << argv[0] << " pid: " << pid << std::endl;
#endif
        // add process to cgroup (echo PID > cgroup.procs)
        cge.add_process(pid);
        cgf.add_process(pid);
        // END PARENT PROCESS
    }
}

void Process::kill()
{
    cgf.freeze();
    cgf.kill_all();
    cgf.unfreeze();
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

void Process::populated_cb(bool populated)
{
    if(!populated){
        part.proc_exit_cb(*this);
    }
}

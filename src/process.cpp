#include "process.hpp"
#include <system_error>
#include <functional>

Process::Process(ev::loop_ref loop,
                 std::string partition_cgrp_name,
                 std::vector<std::string> argv,
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
    , cgroup(loop, true, "test", partition_cgrp_name)
{
    exec();
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
    std::cerr<< __PRETTY_FUNCTION__ << " " + argv[1] <<std::endl;
#endif
    //TODO pipe

    // freeze cgroup
    freeze();


    // cast string to char*
    std::vector<char*> cstrings;
    cstrings.reserve( argv.size()+1 );

    for(size_t i = 0; i < argv.size(); ++i)
        cstrings.push_back(const_cast<char*>( argv[i].c_str() ));
    cstrings.push_back( (char*)NULL );

    // create new process
    pid = CHECK(vfork());

    // launch new process
    if( pid == 0 ){
        // CHILD PROCESS
        CHECK(execv( cstrings[0], &cstrings[0] ));
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

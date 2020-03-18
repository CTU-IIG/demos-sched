#include "process.hpp"
#include "partition.hpp"
#include <functional>
#include <system_error>
#include <sys/eventfd.h>

using namespace std::placeholders;
using namespace std;

Process::Process(ev::loop_ref loop,
                 std::string name,
                 Partition &part,
                 string argv,
                 std::chrono::nanoseconds budget,
                 std::chrono::nanoseconds budget_jitter,
                 bool contionuous)
    : loop(loop)
    , completed_w(loop)
    , part(part)
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
    // std::cerr<<__PRETTY_FUNCTION__<<" "<<name<<std::endl;
    freeze();
    completed_w.set(std::bind(&Process::completed_cb, this));
    //completed_w.start();
    efd_continue = CHECK(eventfd(0, EFD_NONBLOCK));
}

void Process::exec()
{
    // TODO pipe

    // create new process
    pid = CHECK(vfork());
    running = true;

    // launch new process
    if (pid == 0) {
        // CHILD PROCESS
        string env = "DEMOS_EFD=" + to_string(completed_w.get_fd())
                + "," + to_string(efd_continue);
        char *const envp[2] = {const_cast<char*>(env.c_str()), nullptr};
        CHECK(execle("/bin/sh", "/bin/sh", "-c", argv.c_str(), nullptr, envp));
        // END CHILD PROCESS
    } else {
        // PARENT PROCESS
#ifdef VERBOSE
        std::cerr << __PRETTY_FUNCTION__ << " " << argv << " pid: " << pid << std::endl;
#endif
        // add process to cgroup (echo PID > cgroup.procs)
        cge.add_process(pid);
        cgf.add_process(pid);
        // END PARENT PROCESS
    }
}

void Process::kill()
{
    if (running) {
        cgf.freeze();
        cgf.kill_all();
        cgf.unfreeze();
    }
}

void Process::freeze()
{
    cgf.freeze();
}

void Process::unfreeze()
{
    uint64_t buf = 1;
    CHECK(write(efd_continue, &buf, sizeof(buf)));
    cgf.unfreeze();
}

std::chrono::nanoseconds Process::get_actual_budget()
{
    return actual_budget;
}

bool Process::is_completed() const
{
    return completed;
}

void Process::mark_completed()
{
    if (!continuous)
        completed = true;
}

void Process::mark_uncompleted()
{
    completed = false;
}

bool Process::is_running()
{
    return running;
}

pid_t Process::get_pid() const
{
    return pid;
}

void Process::populated_cb(bool populated)
{
    if (!populated) {
        running = false;
        part.proc_exit_cb(*this);
    }
}

void Process::completed_cb()
{
    //cout<<__PRETTY_FUNCTION__<<endl;
    // switch to next process
    completed = true;
    part.completed_cb();
}

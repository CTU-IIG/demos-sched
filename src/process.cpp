#include "process.hpp"
#include "log.hpp"
#include "partition.hpp"
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <system_error>

using namespace std::placeholders;
using namespace std;

Process::Process(ev::loop_ref loop,
                 std::string name,
                 Partition &part,
                 string argv,
                 std::chrono::nanoseconds budget,
                 std::chrono::nanoseconds budget_jitter,
                 bool continuous,
                 bool has_initialization)
    : loop(loop)
    , completed_w(loop)
    , child_w(loop)
    , efd_continue(CHECK(eventfd(0, EFD_SEMAPHORE)))
    , part(part)
    , cge(loop, part.cge, name, std::bind(&Process::populated_cb, this, _1))
    , cgf(part.cgf, name)
    , argv(argv)
    , budget(budget)
    , budget_jitter(budget_jitter)
    , actual_budget(budget)
    , continuous(continuous)
    , has_initialization(has_initialization)
//, cgroup(loop, true, argv[0], partition_cgrp_name)
// TODO regex to cut argv[0] at "/"
//, cgroup(loop, true, "test", fd_cpuset_procs, partition_cgrp_name)
//, cge(loop, part.cgroup, name, std::bind(&Process::populated_cb, this, _1))
{
    // std::cerr<<__PRETTY_FUNCTION__<<" "<<name<<std::endl;
    freeze();
    completed_w.set(std::bind(&Process::completed_cb, this));
    completed_w.start();
    child_w.set<Process, &Process::child_terminated_cb>(this);
}

void Process::exec()
{
    // TODO pipe

    // create new process
    pid = CHECK(fork());

    // launch new process
    if (pid == 0) {
        // CHILD PROCESS
        char val[100];
        // passed descriptor is used for communication with demos process library
        sprintf(val, "%d,%d", completed_w.get_fd(), efd_continue);
        CHECK(setenv("DEMOS_FDS", val, 1));
        CHECK(execl("/bin/sh", "/bin/sh", "-c", argv.c_str(), nullptr));
        // END CHILD PROCESS
    } else {
        // PARENT PROCESS
        logger->debug("Running '{}' as PID {}", argv, pid);
        child_w.start(pid, 0);
        // add process to cgroup (echo PID > cgroup.procs)
        cge.add_process(pid);
        cgf.add_process(pid);
        // END PARENT PROCESS
    }
}

void Process::kill()
{
    if (is_running()) {
        cgf.freeze();
        cgf.kill_all();
        cgf.unfreeze();
    }
}

void Process::freeze()
{
    cgf.freeze();
    if (is_running()) {
        logger->trace("Frozen process '{}'", pid);
    }
}

void Process::unfreeze()
{
    logger->trace("Resuming process '{}'", pid);
    uint64_t buf = 1;
    if (demos_completed) {
        CHECK(write(efd_continue, &buf, sizeof(buf)));
        demos_completed = false;
    }
    cgf.unfreeze();
}

std::chrono::nanoseconds Process::get_actual_budget()
{
    return actual_budget;
}

bool Process::needs_initialization() const {
    return has_initialization;
}

bool Process::is_completed() const
{
    return !is_running() || completed;
}

void Process::mark_completed()
{
    if (!continuous) completed = true;
}

void Process::mark_uncompleted()
{
    completed = false;
}

bool Process::is_running() const
{
    return pid >= 0;
}

pid_t Process::get_pid() const
{
    return pid;
}

void Process::populated_cb(bool populated)
{
    if (!populated) {
        logger->debug("Cgroup of process '{}' not populated (cmd: '{}')", pid, argv);
        pid = -1;
        part.proc_exit_cb(*this);
    }
}

/**
 * Called when the process signalizes completion in the current window
 * by calling demos_completed().
 *
 * This either means that initialization finished (if 'init' param in config was set to true),
 * or that the work for current window is done.
 */
void Process::completed_cb()
{
    logger->trace("Process '{}' completed (cmd: '{}')", pid, argv);

    // switch to next process
    completed = true;
    demos_completed = true;
    part.completed_cb();
}

void Process::child_terminated_cb(ev::child &w, int revents)
{
    w.stop();
    int wstatus = w.rstatus;
    if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
        logger->warn(
          "Process '{}' exited with status {} (cmd: '{}')", pid, WEXITSTATUS(wstatus), argv);
    } else if (WIFSIGNALED(wstatus)) {
        logger->warn(
          "Process '{}' terminated by signal {} (cmd: '{}')", pid, WTERMSIG(wstatus), argv);
    }
}

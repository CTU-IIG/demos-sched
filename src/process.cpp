#include "process.hpp"
#include "log.hpp"
#include "partition.hpp"
#include <cassert>
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
                 bool has_initialization)
    : loop(loop)
    , efd_continue(CHECK(eventfd(0, EFD_SEMAPHORE)))
    , part(part)
    , cge(loop, part.cge, name, std::bind(&Process::populated_cb, this, _1))
    , cgf(part.cgf, name)
    , argv(argv)
    , budget(budget)
    , actual_budget(budget)
    , has_initialization(has_initialization)
{
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
    running = true;
    original_process_running = true;
    killed = false;

    // launch new process
    if (pid == 0) {
        // CHILD PROCESS
        char val[100];
        // pass configuration to the process library
        // passed descriptors are used for communication with the library:
        //  - completed_fd is used to read completion messages from process
        //  - efd_continue is used to signal continuation to process
        sprintf(val, "%d,%d,%d", completed_w.get_fd(), efd_continue, has_initialization);
        CHECK(setenv("DEMOS_PARAMETERS", val, 1));
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
    if (!is_running()) return;
    cgf.freeze();
    cgf.kill_all();
    cgf.unfreeze();
    killed = true;
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
    assert(is_running());
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

bool Process::needs_initialization() const
{
    return has_initialization;
}

bool Process::is_pending() const
{
    return is_running() && !completed;
}

void Process::mark_completed()
{
    completed = true;
}

void Process::mark_uncompleted()
{
    completed = false;
}

bool Process::is_running() const
{
    return running;
}

pid_t Process::get_pid() const
{
    return pid;
}

/** Called when the cgroup is empty and we want to signal it to partition. */
void Process::handle_end()
{
    running = false;
    part.proc_exit_cb();
}

/**
 * Called when the population of cgroup changes
 * (either it is now empty, or new process was added).
 *
 * @param populated - true if the cgroup currently contains any processes
 */
void Process::populated_cb(bool populated)
{
    if (populated) return;
    if (original_process_running) {
        /* child_terminated_cb was not called yet; given that the
           cgroup is now empty, it means that the termination event
           should be pending; the callback will have more information
           about how the process ended, so we'll not call proc_exit_cb
           yet and leave that to the termination callback */
        return;
    }

    logger->trace("Cgroup for process '{}' is empty", pid);
    handle_end();
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

/** Called when our spawned child process terminates. */
void Process::child_terminated_cb(ev::child &w, int revents)
{
    w.stop();
    int wstatus = w.rstatus;
    // clang-format off
    if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
        logger->warn("Process '{}' exited with status {} (partition: '{}', cmd: '{}')",
                     pid, WEXITSTATUS(wstatus), part.get_name(), argv);
    } else if (WIFSIGNALED(wstatus) && !killed) { // don't warn if killed by scheduler
        logger->warn("Process '{}' terminated by signal {} (partition: '{}', cmd: '{}')",
                     pid, WTERMSIG(wstatus), part.get_name(), argv);
    } else {
        logger->debug("Process '{}' exited (partition '{}', cmd: '{}')",
                      pid, part.get_name(), argv);
    }
    // clang-format on

    original_process_running = false;

    if (cge.read_populated_status()) {
        // direct child exited, but the cgroup is not empty
        // most probably, the child process left behind an orphaned child
        logger->trace("Original process '{}' ended, but it has orphaned children", pid);
        // we'll let `populated_cb` call `handle_end()` when the cgroup is emptied
    } else {
        handle_end(); // cgroup is empty
    }
}

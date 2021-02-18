#ifndef PROCESS_H
#define PROCESS_H

#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <vector>

#include "cgroup.hpp"
#include "demossched.hpp"
#include "evfd.hpp"
#include "timerfd.hpp"

class Partition;

/**
 * Represents a single system process (started as a shell command), with a fixed time budget.
 * Process budget is per window cycle - in each window cycle, given process can
 * only run that long before it is suspended and the next process from the parent
 * partition is ran (if such process exists).
 *
 * Starts in frozen state.
 */
class Process
{
public:
    Process(ev::loop_ref loop,
            std::string name,
            Partition &partition,
            std::string argv,
            std::chrono::nanoseconds budget,
            bool has_initialization = false);

    /** Spawns the underlying system process. */
    void exec();

    /**
     * Kills the process, and all children in its cgroup.
     *
     * Does not wait for exit, you should wait for the exit of all processes
     * on the parent partition using `set_empty_cb`.
     */
    void kill();

    /**
     * Suspends this process and all children.
     * Internally, this freezes the underlying cgroup.
     */
    void suspend();
    /**
     * Resumes this process and all children.
     * Internally, this unfreezes the underlying cgroup.
     */
    void resume();

    std::chrono::nanoseconds get_actual_budget();
    pid_t get_pid() const;
    bool needs_initialization() const;
    bool is_running() const;
    bool is_pending() const;

    void mark_completed();
    void mark_uncompleted();

private:
    ev::loop_ref loop;
    ev::evfd completed_w{ loop };
    ev::child child_w{ loop };
    int efd_continue; // new period eventfd

    Partition &part;
    CgroupEvents cge;
    CgroupFreezer cgf;

    std::string argv;
    std::chrono::nanoseconds budget;
    std::chrono::nanoseconds actual_budget;
    bool has_initialization;
    bool completed = false;
    bool demos_completed = false;
    // cannot be replaced by `pid >= 0`, as we want
    //  to keep pid even after process exits, to be
    //  able to correctly handle some delayed events
    /** Indicates if the process or any of its (possibly detached) children are running. */
    bool running = false;
    /** Indicates if the explicitly spawned process is running */
    bool original_process_running = false;
    /** Indicates if the process was killed using `Process::kill()`. */
    bool killed = false;
    pid_t pid = -1;

    void handle_end();
    void populated_cb(bool populated);
    void completed_cb();
    void child_terminated_cb(ev::child &w, int revents);
};

#endif

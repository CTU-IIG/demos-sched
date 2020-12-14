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
            std::chrono::nanoseconds budget_jitter = std::chrono::nanoseconds(0),
            bool continuous = false,
            bool has_initialization = false);

    Process(ev::loop_ref loop,
            std::string name,
            Partition &partition,
            std::string argv,
            std::chrono::nanoseconds budget,
            bool has_initialization)
        : Process(loop,
                  name,
                  partition,
                  argv,
                  budget,
                  std::chrono::nanoseconds(0),
                  false,
                  has_initialization)
    {}

    void exec();
    void kill();

    void freeze();
    void unfreeze();
    std::chrono::nanoseconds get_actual_budget();

    pid_t get_pid() const;
    bool needs_initialization() const;
    bool is_running() const;
    bool is_completed() const;

    void mark_completed();
    void mark_uncompleted();

    // delete copy constructor
    //        Process(const Process&) = delete;
    //        Process& operator=(const Process&) = delete;
private:
    ev::loop_ref loop;
    ev::evfd completed_w;
    ev::child child_w;
    int efd_continue; // new period eventfd

    Partition &part;
    CgroupEvents cge;
    CgroupFreezer cgf;

    void populated_cb(bool populated);
    void completed_cb();
    void child_terminated_cb(ev::child &w, int revents);

    // std::string partition_cgrp_name;
    std::string argv;
    std::chrono::nanoseconds budget;
    std::chrono::nanoseconds budget_jitter;
    std::chrono::nanoseconds actual_budget;
    bool completed = false;
    bool demos_completed = false;
    bool continuous;
    bool has_initialization;
    pid_t pid = -1;
    // Cgroup cgroup;
};

#endif

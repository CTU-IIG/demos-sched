#ifndef PROCESS_H
#define PROCESS_H

#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <vector>

#include "cgroup.hpp"
#include "demossched.hpp"
#include "timerfd.hpp"
#include "evfd.hpp"

class Partition;

class Process
{
public:
    Process(ev::loop_ref loop,
            std::string name,
            Partition &partition,
            std::string argv,
            std::chrono::nanoseconds budget,
            std::chrono::nanoseconds budget_jitter = std::chrono::nanoseconds(0),
            bool contionuous = false);

    // bool is_completed();
    void exec();
    void kill();

    void freeze();
    void unfreeze();
    std::chrono::nanoseconds get_actual_budget();

    bool is_completed() const;
    void mark_completed();
    void mark_uncompleted();

    bool is_running();

    pid_t get_pid() const;

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

    //        std::string partition_cgrp_name;
    std::string argv;
    std::chrono::nanoseconds budget;
    std::chrono::nanoseconds budget_jitter;
    std::chrono::nanoseconds actual_budget;
    bool completed = false;
    bool demos_completed = false;
    bool continuous;
    bool running = false;
    pid_t pid = -1;
    //        Cgroup cgroup;
};

#endif

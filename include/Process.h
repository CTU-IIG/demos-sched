#ifndef PROCESS_H
#define PROCESS_H

#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/timerfd.h>
#include <fcntl.h>

#include "timerfd.hpp"
#include "functions.h"

// TODO exception after fork ??

// paths to cgroups
extern std::string freezer;
extern std::string cpuset;
extern std::string cgrp;

class Process
{
    private:
        std::string name;
        std::vector<std::string> argv;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::nanoseconds budget;
        std::chrono::nanoseconds budget_jitter;
        std::chrono::nanoseconds actual_budget;
        bool completed = false;
        pid_t pid = -1;
        // TODO why ev::timerfd timer doesn't work??? 
        ev::timerfd *timer_ptr = new ev::timerfd;
        int fd_freez_procs = -1;
        int fd_freez_state = -1;
    public:
        Process(std::string name,
                std::vector<std::string> argv,
                std::chrono::steady_clock::time_point start_time,
                std::chrono::nanoseconds budget,
                std::chrono::nanoseconds budget_jitter = std::chrono::nanoseconds(0) );
        ~Process();

        // testing
        void start_timer(std::chrono::nanoseconds timeout);

        bool is_completed();
        void exec();
        void freeze(); // echo FROZEN > freezer.state
        void unfreeze(); // echo THAWED > freezer.state
        void recompute_budget();
        std::chrono::nanoseconds get_actual_budget();
        void timeout_cb (ev::io &w, int revents);
};

#endif

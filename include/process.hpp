#ifndef PROCESS_H
#define PROCESS_H

#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/timerfd.h>
#include <fcntl.h>
#include <chrono>

#include "timerfd.hpp"
#include "cgroup.hpp"
#include "demossched.hpp"

// TODO exception after fork ??

class Process : protected DemosSched
{
    public:
        Process(std::string name,
                std::vector<std::string> argv,
                std::chrono::nanoseconds budget,
                std::chrono::nanoseconds budget_jitter = std::chrono::nanoseconds(0) );

        // testing
        void start_timer(std::chrono::nanoseconds timeout);

        bool is_completed();
        void exec();
        void freeze();
        void unfreeze();
        void recompute_budget();
        std::chrono::nanoseconds get_actual_budget();
        void timeout_cb (ev::timerfd &t);

        // delete copy constructor
        Process(const Process&) = delete;
        Process& operator=(const Process&) = delete;

    private:

        std::string name;
        std::vector<std::string> argv;
        std::chrono::nanoseconds budget;
        std::chrono::nanoseconds budget_jitter;
        std::chrono::nanoseconds actual_budget;
        bool completed = false;
        pid_t pid = -1;
        ev::timerfd timer;
        Cgroup cgroup;

};

#endif
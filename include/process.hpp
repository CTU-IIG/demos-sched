#ifndef PROCESS_H
#define PROCESS_H

#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/timerfd.h>
#include <fcntl.h>

#include "timerfd.hpp"
#include "functions.hpp"
#include "freezer.hpp"

// TODO exception after fork ??

class Process
{
    public:
        Process(std::string name,
                std::vector<std::string> argv,
                std::chrono::steady_clock::time_point start_time,
                std::chrono::nanoseconds budget,
                std::chrono::nanoseconds budget_jitter = std::chrono::nanoseconds(0) );
        //~Process();
        
        // kill and delete cgroup
        void clean();

        // testing
        void start_timer(std::chrono::nanoseconds timeout);

        bool is_completed();
        void exec();
        void freeze();
        void unfreeze();
        void recompute_budget();
        std::chrono::nanoseconds get_actual_budget();
        void timeout_cb (ev::io &w, int revents);

        static void set_cgroup_paths(std::string freezer, std::string cpuset);

        static std::string freezer_path;
        static std::string cpuset_path;

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
        Freezer freezer;

};

#endif

#ifndef SLICE_HPP
#define SLICE_HPP

#include "partition.hpp"
#include "timerfd.hpp"
#include "demossched.hpp"
#include <bitset>
#include <ev++.h>
#include <chrono>

// maximum supported number of processors
#define MAX_NPROC 8

// cpu usage mask
typedef std::bitset<MAX_NPROC> Cpu;

class Slice : protected DemosSched
{
public:
    Slice(ev::loop_ref loop, Partition &sc, Partition &be, Cpu cpus);

    Partition &sc;
    Partition &be;
    Cpu cpus;
    void start();
    void stop();
    void update_timeout(std::chrono::steady_clock::time_point actual_time);
private:
    Process *current_proc = nullptr;
    std::chrono::steady_clock::time_point timeout;
    ev::timerfd timer;
    void timeout_cb();
    void move_proc_and_start_timer(Partition &p);
};

#endif // SLICE_HPP

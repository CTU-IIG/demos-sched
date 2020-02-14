#ifndef SLICE_HPP
#define SLICE_HPP

#include "partition.hpp"
#include "timerfd.hpp"
#include "demossched.hpp"
#include <ev++.h>
#include <chrono>

typedef std::list<Partition> Partitions;

class Slice
{
public:
    Slice(ev::loop_ref loop, std::chrono::steady_clock::time_point start_time,
          Partition &sc, Partition &be, std::string cpus = "0");

    Partition &sc;
    Partition &be;
    //Cpu cpus;
    std::string cpus;
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

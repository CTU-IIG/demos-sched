#ifndef SLICE_HPP
#define SLICE_HPP

#include "partition.hpp"
#include "timerfd.hpp"
#include "demossched.hpp"
#include <ev++.h>
#include <chrono>
#include <functional>

typedef std::list<Partition> Partitions;

class Slice
{
public:
    Slice(ev::loop_ref loop, std::chrono::steady_clock::time_point start_time,
          Partition &sc, Partition &be, std::string cpus = "0");

    void bind_empty_cb( std::function<void()> );

    Partition &sc;
    Partition &be;
    //Cpu cpus;
    std::string cpus;
    void start();
    void stop();
    bool is_empty();
    void update_timeout(std::chrono::steady_clock::time_point actual_time);

    //void delete_part_cb();
private:
    Process *current_proc = nullptr;
    std::chrono::steady_clock::time_point timeout;
    ev::timerfd timer;
    void timeout_cb();
    void move_proc_and_start_timer(Partition &p);
    void empty_partition_cb();
    bool empty = true;

    void default_empty_cb(){}
    std::function<void()> empty_cb = std::bind(&Slice::default_empty_cb, this);
};

#endif // SLICE_HPP

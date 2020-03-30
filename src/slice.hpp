#ifndef SLICE_HPP
#define SLICE_HPP

#include "demossched.hpp"
#include "partition.hpp"
#include "timerfd.hpp"
#include <chrono>
#include <ev++.h>
#include <functional>

typedef std::list<Partition> Partitions;

class Slice
{
public:
    Slice(ev::loop_ref loop,
          std::chrono::steady_clock::time_point start_time,
          Partition *sc,
          Partition *be,
          std::string cpus = "0");

    Slice(const Slice&) = delete;
    const Slice& operator=(const Slice&) = delete;

    void set_empty_cb(std::function<void()>);

    Partition *sc;
    Partition *be;
    std::string cpus;
    void start();
    void stop();
    bool is_empty();
    void update_timeout(std::chrono::steady_clock::time_point actual_time);

private:
    Process *current_proc = nullptr;
    std::chrono::steady_clock::time_point timeout;
    ev::timerfd timer;
    void timeout_cb();
    void move_proc_and_start_timer(Partition *p);
    void empty_partition_cb();
    bool empty = false;

    std::function<void()> empty_cb = nullptr;
};

#endif // SLICE_HPP

#ifndef SLICE_HPP
#define SLICE_HPP

#include "cpu_set.hpp"
#include "demossched.hpp"
#include "partition.hpp"
#include "timerfd.hpp"
#include <chrono>
#include <ev++.h>
#include <functional>

using Partitions = std::list<Partition>;
using time_point = std::chrono::steady_clock::time_point;

/**
 * Associates partitions/processes with a given CPU core set. Always scheduled as part of a Window.
 *
 * In each slice, there are potentially 2 partitions:
 * - `sc_partition` = safety-critical partition
 * - `be_partition` = best-effort partittion
 *
 * First, safety-critical partition is ran; after it finishes (either its time
 * budget is exhausted, or it completes), best-effort partition is started.
 */
class Slice
{
public:
    Slice(ev::loop_ref loop,
          Partition *sc,
          Partition *be,
          cpu_set cpus = cpu_set(0x1));

    Slice(const Slice &) = delete;
    const Slice &operator=(const Slice &) = delete;

    Partition *sc;
    Partition *be;
    const cpu_set cpus;

    void start(time_point current_time);
    void stop();
    bool is_empty();
    void set_empty_cb(std::function<void()>);

private:
    Process *current_proc = nullptr;
    // will be overwritten in start(...), value is not important
    time_point timeout = time_point::min();
    ev::timerfd timer;
    bool empty = false;
    std::function<void()> empty_cb = nullptr;

    void schedule_next();
    void move_proc_and_start_timer(Partition *p);
    void empty_partition_cb();
};

#endif // SLICE_HPP

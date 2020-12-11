#ifndef SLICE_HPP
#define SLICE_HPP

#include "cpu_set.hpp"
#include "demossched.hpp"
#include "partition.hpp"
#include "timerfd.hpp"
#include <chrono>
#include <ev++.h>
#include <functional>

typedef std::list<Partition> Partitions;

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
          std::chrono::steady_clock::time_point start_time,
          Partition *sc,
          Partition *be,
          cpu_set cpus = cpu_set(0x1));

    Slice(const Slice &) = delete;
    const Slice &operator=(const Slice &) = delete;

    void set_empty_cb(std::function<void()>);

    Partition *sc;
    Partition *be;
    const cpu_set cpus;
    void start();
    void stop();
    bool is_empty();
    void update_timeout(std::chrono::steady_clock::time_point actual_time);

private:
    Process *current_proc = nullptr;
    std::chrono::steady_clock::time_point timeout;
    ev::timerfd timer;
    void schedule_next();
    void move_proc_and_start_timer(Partition *p);
    void empty_partition_cb();
    bool empty = false;

    std::function<void()> empty_cb = nullptr;
};

#endif // SLICE_HPP

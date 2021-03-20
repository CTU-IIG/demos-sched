#pragma once

#include "check_lib.hpp"
#include "cpu_set.hpp"
#include "partition.hpp"
#include "timerfd.hpp"
#include <chrono>
#include <ev++.h>
#include <functional>

using time_point = std::chrono::steady_clock::time_point;

/**
 * Associates partitions/processes with a given CPU core set. Always scheduled as part of a Window.
 *
 * In each slice, there are potentially 2 partitions:
 * - `sc_partition` = safety-critical partition
 * - `be_partition` = best-effort partition
 *
 * First, safety-critical partition is ran; after it finishes (either its time
 * budget is exhausted, or it completes), best-effort partition is started.
 */
class Slice
{
public:
    Slice(ev::loop_ref loop, Partition *sc, Partition *be, cpu_set cpus = cpu_set(0x1));

    Slice(const Slice &) = delete;
    const Slice &operator=(const Slice &) = delete;

    // these should really be std::optional, but since C++
    //  is a half-assed, optional references are not a thing
    /** Safety-critical partition running in this slice. */
    Partition *const sc;
    /** Best-effort partition running in this slice. */
    Partition *const be;
    const cpu_set cpus;

    void start(time_point current_time);
    void stop();

private:
    Process *current_proc = nullptr;
    // will be overwritten in start(...), value is not important
    time_point timeout = time_point::min();
    ev::timerfd timer;
    // cached, so that we don't create new std::function each time we set the callback
    std::function<void()> completion_cb_cached = [this] { schedule_next(); };

    bool load_next_process();
    void start_current_process();
    void schedule_next();
};

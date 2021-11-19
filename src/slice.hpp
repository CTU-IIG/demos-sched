#pragma once

#include "cpufreq_policy.hpp"
#include "lib/cpu_set.hpp"
#include "partition.hpp"
#include "timerfd.hpp"
#include <chrono>
#include <ev++.h>
#include <functional>

class PowerPolicy;

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
    Slice(ev::loop_ref loop,
          PowerPolicy &power_policy,
          std::function<void(Slice &, time_point)> sc_done_cb,
          Partition *sc,
          Partition *be,
          std::optional<CpuFrequencyHz> req_freq,
          cpu_set cpus = cpu_set(0x1));

    Slice(const Slice &) = delete;
    const Slice &operator=(const Slice &) = delete;

    // these should really be std::optional, but optional references are not really a thing in C++
    /** Safety-critical partition running in this slice. */
    Partition *const sc;
    /** Best-effort partition running in this slice. */
    Partition *const be;
    const cpu_set cpus;
    const std::optional<CpuFrequencyHz> requested_frequency;


    /**
     * Starts execution of SC partition, if present. Calls sc_done_cb
     *  when done (or when SC partition is not present).
     */
    void start_sc(time_point current_time);
    /** Starts execution of BE partition, if present. */
    void start_be(time_point current_time);
    void stop(time_point current_time);

private:
    PowerPolicy &power_policy;
    std::function<void(Slice &, time_point)> sc_done_cb;
    Process *running_process = nullptr;
    Partition *running_partition = nullptr;
    // will be overwritten in start(...), value is not important
    time_point timeout = time_point::min();
    ev::timerfd timer;
    // cached, so that we don't create new std::function each time we set the callback
    Partition::CompletionCb completion_cb_cached;

    void schedule_next(time_point current_time);
    void start_partition(Partition *part, time_point current_time, bool move_to_first_proc);
    void stop_current_process(bool mark_completed);
    bool load_next_process(time_point current_time);
    void start_next_process(time_point current_time);
};

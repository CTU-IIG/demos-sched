#pragma once

#include "slice.hpp"
#include <chrono>
#include <ev++.h>
#include <list>
#include <optional>
#include <vector>

class Window;
class PowerPolicy;

using Windows = std::list<Window>;
using time_point = std::chrono::steady_clock::time_point;

/**
 * Represents a time interval when given slices are running on the CPU. Has a
 * defined duration (window length). Each window can contain multiple slices.
 */
class Window
{
private:
    ev::loop_ref loop;
    uint64_t finished_sc_partitions = 0;
    PowerPolicy &power_policy;
    bool stopping = false;

public:
    const std::chrono::milliseconds length;
    // use std::list as Slice doesn't have move and copy constructors
    std::list<Slice> slices{};
    std::vector<bool> to_be_skipped;

    Window(ev::loop_ref loop, std::chrono::milliseconds length, PowerPolicy &power_policy);

    Slice &add_slice(Partition *sc, Partition *be, const cpu_set &cpus, std::optional<CpuFrequencyHz> req_freq);
    [[nodiscard]] bool has_sc_finished() const;
    void start(time_point current_time);
    void stop(time_point current_time);

private:
    void slice_sc_end_cb([[maybe_unused]] Slice &slice, time_point current_time);
};

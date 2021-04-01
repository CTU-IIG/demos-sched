#pragma once

#include "check_lib.hpp"
#include "slice.hpp"
#include <chrono>
#include <ev++.h>
#include <list>

class Window;

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

public:
    Window(ev::loop_ref loop, std::chrono::nanoseconds length);

    const std::chrono::nanoseconds length;
    // use std::list as we don't have move and copy constructors
    std::list<Slice> slices;

    void add_slice(Partition *sc, Partition *be, const cpu_set &cpus);
    void start(time_point current_time);
    void stop();

private:
    void slice_sc_end_cb([[maybe_unused]] Slice *slice, time_point current_time);
};

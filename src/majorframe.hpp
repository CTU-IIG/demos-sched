#pragma once

#include "timerfd.hpp"
#include "window.hpp"
#include <ev++.h>
#include <list>

using time_point = std::chrono::steady_clock::time_point;

/**
 * Container for all time windows.
 *
 * Switches between windows in a cycle (in order, starting from the first one).
 */
class MajorFrame
{
public:
    /**
     * @param loop - main ev loop
     * @param windows - scheduled windows
     * @param window_sync_message - if not empty, this message is printed to stdout
     *  at the beginning of each window
     * @param mf_sync_message - if not empty, this message is printed to stdout
     *  at the beginning of each major frame
     */
    MajorFrame(ev::loop_ref loop,
               Windows &&windows,
               std::string window_sync_message,
               std::string mf_sync_message);

    void start(time_point start_time);
    /** Stops the window scheduler. If not running, this is a noop. */
    void stop(time_point current_time);

    /**
     * Compares cpu_sets of all slices containing given partition
     * and returns pointer to the largest one, or nullptr if no such slice exists.
     */
    const cpu_set *find_widest_cpu_set(Partition &partition);

private:
    ev::timerfd timer;
    Windows windows;
    Windows::iterator current_win;
    // will be overwritten in start(...), value is not important
    time_point timeout = time_point::min();
    const std::string window_sync_message;
    const std::string mf_sync_message;

    void move_to_next_window();
    void timeout_cb();
};

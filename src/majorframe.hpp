#ifndef MAJORFRAME_HPP
#define MAJORFRAME_HPP

#include "demossched.hpp"
#include "timerfd.hpp"
#include "window.hpp"
#include <ev++.h>
#include <list>

using Windows = std::vector<std::unique_ptr<Window>>;
using time_point = std::chrono::steady_clock::time_point;

/**
 * Container for all time windows.
 *
 * Switches between windows in a cycle (in order, starting from the first one).
 */
class MajorFrame
{
public:
    MajorFrame(ev::loop_ref loop, Windows &&windows);

    void start(time_point start_time);
    void stop();

private:
    ev::timerfd timer;
    Windows windows;
    Windows::iterator current_win;
    // will be overwritten in start(...), value is not important
    time_point timeout = time_point::min();

    void move_to_next_window();
    void timeout_cb();
};

#endif // MAJORFRAME_HPP

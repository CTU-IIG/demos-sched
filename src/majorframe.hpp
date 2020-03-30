#ifndef MAJORFRAME_HPP
#define MAJORFRAME_HPP

#include "demossched.hpp"
#include "timerfd.hpp"
#include "window.hpp"
#include <ev++.h>
#include <list>

typedef std::vector<std::unique_ptr<Window>> Windows;

class MajorFrame
{
public:
    MajorFrame(ev::loop_ref loop,
               std::chrono::steady_clock::time_point start_time,
               Windows &&windows);

    void move_to_next_window();
    Window &get_current_window();

    void start();
    void stop();

private:
    // std::chrono::nanoseconds length; // do we need this?
    ev::loop_ref loop;
    Windows windows;
    Windows::iterator current;
    ev::timerfd timer{ loop };
    ev::sig sigint{ loop };
    ev::sig sigterm{ loop };
    void timeout_cb();
    void sigint_cb(ev::sig &w, int revents);
    void empty_cb();
    std::chrono::steady_clock::time_point timeout;

    void kill_all();
};

#endif // MAJORFRAME_HPP

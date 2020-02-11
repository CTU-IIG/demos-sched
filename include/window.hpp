#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "slice.hpp"
#include "demossched.hpp"
#include <list>
#include <chrono>
#include <ev++.h>

typedef std::list<Slice> Slices;

class Window : protected DemosSched
{
public:
    Window(ev::loop_ref loop, Slices &slices, std::chrono::nanoseconds length);

    std::chrono::nanoseconds length;
    Slices &slices;

    void start();
    void stop();
    void update_timeout(std::chrono::steady_clock::time_point actual_time);
private:
    ev::loop_ref loop;
};

#endif // WINDOW_HPP

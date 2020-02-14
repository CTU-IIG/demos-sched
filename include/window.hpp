#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "slice.hpp"
#include "demossched.hpp"
#include <list>
#include <chrono>
#include <ev++.h>

typedef std::list<Slice> Slices;

class Window
{
public:
    Window(Slices &slices, std::chrono::nanoseconds length);

    std::chrono::nanoseconds length;
    Slices &slices;

    void start();
    void stop();
    void update_timeout(std::chrono::steady_clock::time_point actual_time);
};

#endif // WINDOW_HPP

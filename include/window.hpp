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
private:
    ev::loop_ref loop;
};

#endif // WINDOW_HPP

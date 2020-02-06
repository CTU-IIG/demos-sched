#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "slice.hpp"
#include "demossched.hpp"
#include <list>
#include <chrono>

typedef std::list<Slice> Slices;

class Window : protected DemosSched
{
public:
    Window(Slices &slices, std::chrono::nanoseconds length);

    std::chrono::nanoseconds length;
    Slices &slices;
};

#endif // WINDOW_HPP

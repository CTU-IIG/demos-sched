#include "window.hpp"


Window::Window(ev::loop_ref loop, Slices &slices, std::chrono::nanoseconds length)
    : length(length)
    , slices(slices)
    , loop(loop)
{

}

void Window::start()
{
    for(Slice &s : slices)
        s.start();
}

void Window::stop()
{
    for(Slice &s : slices)
        s.stop();
}

void Window::update_timeout(std::chrono::steady_clock::time_point actual_time)
{
    for(Slice &s : slices)
        s.update_timeout(actual_time);
}

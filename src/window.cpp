#include "window.hpp"


Window::Window(Slices &slices, std::chrono::nanoseconds length)
    : length(length)
    , slices(slices)
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

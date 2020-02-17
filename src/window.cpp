#include "window.hpp"

using namespace std;

Window::Window(Slices &slices, std::chrono::nanoseconds length)
    : length(length)
    , slices(slices)
{
    for( Slice &s : slices)
        s.bind_empty_cb( bind(&Window::empty_slice_cb, this));
    empty = false;
}

void Window::bind_empty_cb(std::function<void ()> new_empty_cb)
{
    empty_cb = new_empty_cb;
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

bool Window::is_empty()
{
    return empty;
}

void Window::empty_slice_cb()
{
    for( Slice &s : slices )
        if( !s.is_empty() )
            return;
    cerr<< __PRETTY_FUNCTION__ <<endl;
    empty = true;
    // notify major frame that window is empty
    empty_cb();
}

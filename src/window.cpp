#include "window.hpp"

using namespace std;

Window::Window(Slices &&slices, std::chrono::nanoseconds length)
    : length(length)
    , slices(move(slices))
{
    for (auto &s : this->slices) {
        s->set_empty_cb(bind(&Window::empty_slice_cb, this));
    }
    empty = false;
}

void Window::set_empty_cb(std::function<void()> new_empty_cb)
{
    empty_cb = new_empty_cb;
}

void Window::start(std::chrono::steady_clock::time_point current_time)
{
    for (auto &s : slices) {
        s->start(current_time);
    }
}

void Window::stop()
{
    for (auto &s : slices) {
        s->stop();
    }
}

bool Window::is_empty()
{
    return empty;
}

void Window::empty_slice_cb()
{
    for (auto &s : slices) {
        if (!s->is_empty()) return;
    }
    empty = true;
    // notify major frame that window is empty
    empty_cb();
}

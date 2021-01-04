#include "window.hpp"

using namespace std;

Window::Window(Slices &&slices, std::chrono::nanoseconds length)
    : length(length)
    , slices(move(slices))
{}

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
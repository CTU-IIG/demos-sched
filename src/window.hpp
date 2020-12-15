#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "demossched.hpp"
#include "slice.hpp"
#include <chrono>
#include <ev++.h>
#include <list>

typedef std::vector<std::unique_ptr<Slice>> Slices;

/**
 * Represents a time interval when given slices are running on the CPU. Has a
 * defined duration (window length). Each window can contain multiple slices.
 */
class Window
{
public:
    Window(Slices &&slices, std::chrono::nanoseconds length);
    void set_empty_cb(std::function<void()> new_empty_cb);

    std::chrono::nanoseconds length;
    Slices slices;

    void start(std::chrono::steady_clock::time_point current_time);
    void stop();

    bool is_empty();

private:
    void empty_slice_cb();
    bool empty = true;

    std::function<void()> empty_cb = [] {};
};

#endif // WINDOW_HPP

#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "slice.hpp"
#include "demossched.hpp"
#include <list>
#include <chrono>
#include <ev++.h>

typedef std::vector<std::unique_ptr<Slice>> Slices;

class Window
{
public:
    Window(Slices &&slices, std::chrono::nanoseconds length);
    void bind_empty_cb(std::function<void()> new_empty_cb);

    std::chrono::nanoseconds length;
    Slices slices;

    void start();
    void stop();
    void update_timeout(std::chrono::steady_clock::time_point actual_time);

    bool is_empty();
private:
    void empty_slice_cb();
    bool empty = true;

    void default_empty_cb(){}
    std::function<void()> empty_cb = std::bind(&Window::default_empty_cb, this);
};

#endif // WINDOW_HPP

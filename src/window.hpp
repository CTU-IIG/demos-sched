#pragma once

#include "check_lib.hpp"
#include "slice.hpp"
#include <chrono>
#include <ev++.h>
#include <list>

using Slices = std::vector<std::unique_ptr<Slice>>;

/**
 * Represents a time interval when given slices are running on the CPU. Has a
 * defined duration (window length). Each window can contain multiple slices.
 */
class Window
{
public:
    Window(Slices &&slices, std::chrono::nanoseconds length);

    std::chrono::nanoseconds length;
    Slices slices;

    void start(std::chrono::steady_clock::time_point current_time);
    void stop();

private:
};

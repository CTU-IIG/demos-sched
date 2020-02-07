#include "window.hpp"


Window::Window(ev::loop_ref loop, Slices &slices, std::chrono::nanoseconds length)
    : length(length)
    , slices(slices)
    , loop(loop)
{

}

#include "window.hpp"


Window::Window(Slices &slices, std::chrono::nanoseconds length)
    : length(length)
    , slices(slices)
{

}

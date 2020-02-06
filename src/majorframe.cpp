#include "majorframe.hpp"

MajorFrame::MajorFrame(Windows &windows)
    : windows( windows )
    , current( this->windows.begin() )
{
    timer.set(std::bind(&MajorFrame::timeout_cb, this));
}

void MajorFrame::move_to_next_window()
{
    if( ++current == windows.end() )
        current = windows.begin();
}

Window &MajorFrame::get_current_window()
{
    return *current;
}

void MajorFrame::timeout_cb()
{
    // TODO do all window switching stuff

    std::cerr<< "window timeout" << std::endl;
}

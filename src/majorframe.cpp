#include "majorframe.hpp"
#include "log.hpp"

using namespace std;

MajorFrame::MajorFrame(ev::loop_ref loop,
                       Windows &&windows)
    : timer(loop)
    , windows(std::move(windows))
    , current_win(this->windows.begin())
{
    timer.set(bind(&MajorFrame::timeout_cb, this));
}

void MajorFrame::move_to_next_window()
{
    if (++current_win == windows.end()) {
        current_win = windows.begin();
    }
}

void MajorFrame::start(time_point current_time)
{
    current_win->get()->start(current_time);
    timeout = current_time + current_win->get()->length;
    timer.start(timeout);
}

void MajorFrame::stop()
{
    timer.stop();
    current_win->get()->stop();
}

void MajorFrame::timeout_cb()
{
    logger->trace("Window ended, starting next one");

    current_win->get()->stop();
    move_to_next_window();
    start(timeout);
}
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

    for (auto &w : this->windows) {
        w->set_empty_cb(bind(&MajorFrame::empty_cb, this));
    }
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

void MajorFrame::set_completed_cb(std::function<void()> new_completed_cb)
{
    completed_cb = new_completed_cb;
}

void MajorFrame::timeout_cb()
{
    logger->trace("Window ended, starting next one");

    current_win->get()->stop();
    move_to_next_window();
    start(timeout);
}

void MajorFrame::empty_cb()
{
    for (auto &w : windows) {
        if (!w->is_empty()) return;
    }

    logger->debug("All processes exited");
    stop();
    completed_cb();
}

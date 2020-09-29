#include "majorframe.hpp"
#include "log.hpp"

using namespace std;

MajorFrame::MajorFrame(ev::loop_ref loop,
                       std::chrono::steady_clock::time_point start_time,
                       Windows &&windows)
    : loop(loop)
    , windows(std::move(windows))
    , current_win(this->windows.begin())
    , timeout(start_time)
{
    timer.set(bind(&MajorFrame::timeout_cb, this));
    sigint.set<MajorFrame, &MajorFrame::sigint_cb>(this);
    sigint.start(SIGINT);
    sigterm.set<MajorFrame, &MajorFrame::sigint_cb>(this);
    sigterm.start(SIGTERM);

    for (auto &w : this->windows)
        w->set_empty_cb(bind(&MajorFrame::empty_cb, this));
}

void MajorFrame::move_to_next_window()
{
    if (++current_win == windows.end())
        current_win = windows.begin();
}

Window &MajorFrame::get_current_window()
{
    return *(current_win->get());
}

void MajorFrame::start()
{
    current_win->get()->update_timeout(timeout);
    current_win->get()->start();
    timeout += current_win->get()->length;
    timer.start(timeout);
}

void MajorFrame::stop()
{
    timer.stop();
    current_win->get()->stop();
}

void MajorFrame::timeout_cb()
{
    logger->trace(__PRETTY_FUNCTION__);

    current_win->get()->stop();
    move_to_next_window();
    current_win->get()->update_timeout(timeout);
    current_win->get()->start();
    timeout += current_win->get()->length;
    timer.start(timeout);
}

void MajorFrame::kill_all()
{
    for (auto &w : windows) {
        for (auto &s : w->slices) {
            if (s->be) s->be->kill_all();
            if (s->sc) s->sc->kill_all();
        }
    }
}

void MajorFrame::sigint_cb(ev::sig &w, int revents)
{
    stop();
    kill_all();
}

void MajorFrame::empty_cb()
{
    for (auto &w : windows)
        if (!w->is_empty())
            return;

    logger->debug("All processes exited");
    loop.break_loop(ev::ALL);
}

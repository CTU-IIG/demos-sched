#include "majorframe.hpp"

using namespace std;

MajorFrame::MajorFrame(ev::loop_ref loop,
                       std::chrono::steady_clock::time_point start_time,
                       Windows &&windows)
    : loop(loop)
    , windows(std::move(windows))
    , current(this->windows.begin())
    , timeout(start_time)
{
    timer.set(bind(&MajorFrame::timeout_cb, this));
    sigint.set<MajorFrame, &MajorFrame::sigint_cb>(this);
    sigint.start(SIGINT);
    sigterm.set<MajorFrame, &MajorFrame::sigint_cb>(this);
    sigterm.start(SIGTERM);

    for (auto &w : this->windows)
        w->bind_empty_cb(bind(&MajorFrame::empty_cb, this));
}

void MajorFrame::move_to_next_window()
{
    if (++current == windows.end())
        current = windows.begin();
}

Window &MajorFrame::get_current_window()
{
    return *(current->get());
}

void MajorFrame::start()
{
    current->get()->update_timeout(timeout);
    current->get()->start();
    timeout += current->get()->length;
    timer.start(timeout);
}

void MajorFrame::stop()
{
    timer.stop();
    current->get()->stop();
}

void MajorFrame::timeout_cb()
{
#ifdef VERBOSE
    cerr << __PRETTY_FUNCTION__ << endl;
#endif

    current->get()->stop();
    move_to_next_window();
    current->get()->update_timeout(timeout);
    current->get()->start();
    timeout += current->get()->length;
    timer.start(timeout);
}

void MajorFrame::kill_all()
{
    for (auto &w : windows) {
        for (auto &s : w->slices) {
            s->be.kill_all();
            s->sc.kill_all();
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
#ifdef VERBOSE
    cerr << __PRETTY_FUNCTION__ << endl;
#endif
    loop.break_loop(ev::ALL);
}

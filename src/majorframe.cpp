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

const cpu_set *MajorFrame::find_widest_cpu_set(Partition &partition) {
    Partition *p = &partition;
    const cpu_set *best_found = nullptr;
    for (auto &w : windows) {
        for (auto &s : w->slices) {
            if (s->sc != p && s->be != p) continue; // not our partition
            // if we haven't found any cpuset yet, or the compared cpuset has more cores, store it
            if (!best_found || s->cpus.count() > best_found->count()) {
                best_found = &s->cpus;
            }
        }
    }
    return best_found;
}
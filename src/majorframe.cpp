#include "majorframe.hpp"
#include "log.hpp"
#include <iostream>

using namespace std;

MajorFrame::MajorFrame(ev::loop_ref loop,
                       Windows &&windows_,
                       string window_sync_message_,
                       string mf_sync_message_)
    : timer(loop)
    , windows(std::move(windows_))
    , current_win(windows.begin())
    , window_sync_message(std::move(window_sync_message_))
    , mf_sync_message(std::move(mf_sync_message_))
{
    timer.set([this] { timeout_cb(); });
}

void MajorFrame::move_to_next_window()
{
    if (++current_win == windows.end()) {
        current_win = windows.begin();
    }
}

// TODO: add optimization to keep processes running if their
//  partition immediately continues in the next window
void MajorFrame::start(time_point current_time)
{
    if (!mf_sync_message.empty() && current_win == windows.begin()) {
        cout << mf_sync_message << endl;
        // flush to force correct synchronization with other stdout prints from scheduled programs
        cout.flush();
    }
    if (!window_sync_message.empty()) {
        cout << window_sync_message << endl;
        cout.flush();
    }
    // this call make take 100-200 µs due to the blocking
    //  cpufreq write in case frequency is changed here
    current_win->start(current_time);
    timeout = current_time + current_win->length;
    timer.start(timeout);
}

void MajorFrame::stop(time_point current_time)
{
    timer.stop();
    current_win->stop(current_time);
}

void MajorFrame::timeout_cb()
{
    current_win->stop(timeout);
    if (!current_win->has_sc_finished()) {
        logger->warn("Window ended before all SC partitions finished");
    }
    move_to_next_window();
    start(timeout);
}

const cpu_set *MajorFrame::find_widest_cpu_set(Partition &partition)
{
    Partition *p = &partition;
    const cpu_set *best_found = nullptr;
    for (auto &w : windows) {
        for (auto &s : w.slices) {
            if (s.sc != p && s.be != p) continue; // not our partition
            // if we haven't found any cpuset yet, or the compared cpuset has more cores, store it
            if (!best_found || s.cpus.count() > best_found->count()) {
                best_found = &s.cpus;
            }
        }
    }
    return best_found;
}

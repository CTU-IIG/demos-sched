#include "window.hpp"
#include "log.hpp"

Window::Window(ev::loop_ref loop_, std::chrono::milliseconds length_, SchedulerEvents &events)
    : loop(loop_)
    , sched_events(events)
    , length(length_)
{}

Slice &Window::add_slice(Partition *sc, Partition *be, const cpu_set &cpus)
{
    auto sc_cb = [this](Slice &s, time_point t) { slice_sc_end_cb(s, t); };
    return slices.emplace_back(loop, sc_cb, sc, be, cpus);
}

bool Window::has_sc_finished() const
{
    return finished_sc_partitions == slices.size();
}

void Window::start(time_point current_time)
{
    logger->trace("Starting window");
    sched_events.on_window_start(*this);
    sched_events.on_sc_start(*this);
    finished_sc_partitions = 0;
    for (auto &s : slices) {
        s.start_sc(current_time);
    }
}

void Window::stop(time_point current_time)
{
    // when SC partition inside a slice completes at the same moment Window::stop is called,
    //
    stopping = true;
    for (auto &s : slices) {
        s.stop(current_time);
    }
    stopping = false;
    sched_events.on_window_end(*this);
}

void Window::slice_sc_end_cb([[maybe_unused]] Slice &slice, time_point current_time)
{
    finished_sc_partitions++;

    if (stopping) {
        // if the window is currently stopping, don't launch any new partition
        return;
    }

    // option 1) run BE immediately after SC
    // slice->start_be(current_time);

    // option 2) wait until all SC partitions finish
    if (has_sc_finished()) {
        logger->trace("Starting BE partitions");
        sched_events.on_be_start(*this);
        for (auto &sp : slices) {
            sp.start_be(current_time);
        }
    }
}

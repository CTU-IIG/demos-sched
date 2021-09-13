#include "window.hpp"
#include "log.hpp"
#include "power_policy/_power_policy.hpp"

Window::Window(ev::loop_ref loop_, std::chrono::milliseconds length_, PowerPolicy &power_policy)
    : loop(loop_)
    , power_policy(power_policy)
    , length(length_)
{}

Slice &Window::add_slice(Partition *sc, Partition *be, const cpu_set &cpus)
{
    auto sc_cb = [this](Slice &s, time_point t) { slice_sc_end_cb(s, t); };
    return slices.emplace_back(loop, power_policy, sc_cb, sc, be, cpus);
}

bool Window::has_sc_finished() const
{
    return finished_sc_partitions == slices.size();
}

void Window::start(time_point current_time)
{
    TRACE("Starting window");
    finished_sc_partitions = 0;
    for (auto &s : slices) {
        s.start_sc(current_time);
    }
    // call power policy handlers after we start the slices;
    //  this way, even if the CPU frequency switching is slow, the processes are
    //  still executing, although at an incorrect frequency;
    //  see `CpufreqPolicy::write_frequency` for more info
    power_policy.on_window_start(*this);
    power_policy.on_sc_start(*this);
}

void Window::stop(time_point current_time)
{
    // this way, the previous window will run a bit longer
    //  if this call takes a long time to complete
    power_policy.on_window_end(*this);
    // when SC partition inside a slice completes at the same moment Window::stop is called
    //  (typically due to 2 timeouts firing at the same time), `stopping = true` prevents
    //  the next partition from starting execution and then getting immediately stopped
    //  by Window::stop (see slice.cpp for more details)
    stopping = true;
    for (auto &s : slices) {
        s.stop(current_time);
    }
    stopping = false;
}

void Window::slice_sc_end_cb([[maybe_unused]] Slice &slice, time_point current_time)
{
    finished_sc_partitions++;

    if (stopping) {
        // if the window is currently stopping, don't launch any new partition
        return;
    }

    // option 1) run BE immediately after SC
    // per-process frequency validation currently assumes option 2) here,
    //  doesn't work correctly with option 1)
    // slice->start_be(current_time);

    // option 2) wait until all SC partitions finish
    if (has_sc_finished()) {
        TRACE("Starting BE partitions");
        for (auto &sp : slices) {
            sp.start_be(current_time);
        }
        // see `Window::start` for reasoning on why this is called AFTER partition start
        power_policy.on_be_start(*this);
    }
}

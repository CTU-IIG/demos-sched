#include "slice.hpp"
#include <cassert>

/*
TODO: I think there is a subtle race condition here, where
 the window or process timeout could fire and before the process
 is frozen, it signals completion, which will be handled by the event loop
 after new window is started, and the completion signal from the previous
 window will arrive to the new window, and the process will be immediately
 blocked and not run at all during next window.
 */
Slice::Slice(ev::loop_ref loop,
             std::function<void(Slice *, time_point)> sc_done_cb,
             Partition *sc,
             Partition *be,
             cpu_set cpus)
    : sc(sc)
    , be(be)
    , cpus(std::move(cpus))
    , sc_done_cb(std::move(sc_done_cb))
    , timer(loop)
{
    timer.set([this] { schedule_next(); });
}

/** Finds and starts next unfinished process from current_partition. */
void Slice::start_next_process(time_point current_time)
{
    // find next process
    running_process = running_partition->seek_pending_process();
    if (!running_process) {
        // no process found, we're done
        // call cb if running SC partition
        if (running_partition == sc) {
            sc_done_cb(this, current_time);
        }
        return;
    }
    running_process->resume();
    timeout = current_time + running_process->get_actual_budget();
    timer.start(timeout);
}

void Slice::start_partition(Partition *part, time_point current_time, bool move_to_first_proc)
{
    assert(part != nullptr);
    running_partition = part;
    if (part) {
        part->reset(move_to_first_proc, cpus, completion_cb_cached);
    }
    start_next_process(current_time);
}

void Slice::start_sc(time_point current_time)
{
    if (!sc) {
        sc_done_cb(this, current_time);
        return;
    }
    start_partition(sc, current_time, true);
}

void Slice::start_be(time_point current_time)
{
    if (!be) return;
    // don't move to first process, we want to continue from the last one for BE,
    //  as there aren't strict deadlines for BE processes, and it's better if each BE process gets
    //  to run occasionally than if the first one would run all the time and the rest never
    start_partition(be, current_time, false);
}

/*
TODO: When BE partition process is interrupted by window end,
 store how much of its budget it used up, and next time it runs
 in a window, restart the process with lowered budget.
 Otherwise, single long BE process could block all other
 processes in the same partition from executing.
*/
void Slice::stop()
{
    if (running_process) {
        running_process->suspend();
    }
    if (sc) sc->disconnect();
    if (be) be->disconnect();
    timer.stop();
}

// Called as a response to timeout or process completion.
void Slice::schedule_next()
{
    timer.stop();
    running_process->suspend();
    running_process->mark_completed();

    start_next_process(timeout);
}

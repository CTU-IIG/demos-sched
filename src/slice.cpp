#include "slice.hpp"

using namespace std;

/*
TODO: I think there is a subtle race condition here, where
 the window or process timeout could fire and before the process
 is frozen, it signals completion, which will be handled by the event loop
 after new window is started, and the completion signal from the previous
 window will arrive to the new window, and the process will be immediately
 blocked and not run at all during next window.
 */
Slice::Slice(ev::loop_ref loop, Partition *sc, Partition *be, cpu_set cpus)
    : sc(sc)
    , be(be)
    , cpus(cpus)
    , timer(loop)
{
    timer.set(bind(&Slice::schedule_next, this));
}

/**
 * Finds next unfinished process and stores it in `current_proc`.
 * @return true if found, false if no runnable process is available
 */
bool Slice::load_next_process()
{
    Process *proc = nullptr;
    if (sc) proc = sc->find_unfinished_process();
    if (!proc && be) proc = be->find_unfinished_process();
    // nothing to run
    if (!proc) return false;

    current_proc = proc;
    return true;
}

void Slice::start_current_process()
{
    current_proc->unfreeze();
    timeout += current_proc->get_actual_budget();
    timer.start(timeout);
}

void Slice::start(time_point current_time)
{
    if (sc) sc->reset(true, cpus, completion_cb_cached);
    // don't move to first process, we want to continue from the last one for BE,
    //  as there aren't strict deadlines for BE processes, and it's better if each BE process gets
    //  to run occasionally than if the first one would run all the time and the rest never
    // we still need to set completion callback, otherwise BE processes
    //  could not yield to the next BE process
    if (be) be->reset(false, cpus, completion_cb_cached);

    if (!load_next_process()) {
        return; // nothing to run
    }
    timeout = current_time;
    start_current_process();
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
    if (current_proc) {
        current_proc->freeze();
    }
    if (sc) sc->disconnect();
    if (be) be->disconnect();
    timer.stop();
}

// Called as a response to timeout or process completion.
void Slice::schedule_next()
{
    timer.stop();
    current_proc->freeze();
    current_proc->mark_completed();

    if (!load_next_process()) {
        return; // nothing to run
    }
    start_current_process();
}

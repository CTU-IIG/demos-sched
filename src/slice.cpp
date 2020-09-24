#include "slice.hpp"

using namespace std;

Slice::Slice(ev::loop_ref loop,
             chrono::steady_clock::time_point start_time,
             Partition *sc,
             Partition *be,
             cpu_set cpus)
    : sc(sc)
    , be(be)
    , cpus(cpus)
    , timeout(start_time)
    , timer(loop)
{
    if (sc) {
        sc->set_empty_cb(bind(&Slice::empty_partition_cb, this));
        sc->set_complete_cb(bind(&Slice::schedule_next, this));
    }
    if (be)
        be->set_empty_cb(bind(&Slice::empty_partition_cb, this));
    timer.set(bind(&Slice::schedule_next, this));
}

void Slice::set_empty_cb(function<void()> new_empty_cb)
{
    empty_cb = new_empty_cb;
}

void Slice::move_proc_and_start_timer(Partition *p)
{
    if (!p)
        return;
    p->move_to_next_unfinished_proc();
    timeout += p->get_current_proc().get_actual_budget();
    timer.start(timeout);
}

void Slice::empty_partition_cb()
{
    if ( (!sc || sc->is_empty()) && (!be || be->is_empty())) {
#ifdef VERBOSE
        cerr << __PRETTY_FUNCTION__ << endl;
#endif
        empty = true;
        if (empty_cb)
            empty_cb();
    }
}

void Slice::start()
{
    if (sc) {
        sc->clear_completed_flag();
        sc->set_cpus(cpus);
    }
    if (be) {
        be->clear_completed_flag();
        be->set_cpus(cpus);
    }

    if (sc && !sc->is_empty()) {
        sc->move_to_first_proc();
        current_proc = &sc->get_current_proc();
    } else if (be && !be->is_empty()) {
        current_proc = &be->get_current_proc();
    } else {
        return;
    }
    current_proc->unfreeze();
    timeout += current_proc->get_actual_budget();
    timer.start(timeout);
}

void Slice::stop()
{
    if (current_proc)
        current_proc->freeze();
    timer.stop();
}

bool Slice::is_empty()
{
    return empty;
}

void Slice::update_timeout(chrono::steady_clock::time_point actual_time)
{
    timeout = actual_time;
}

// Called as a response to timeout or process completion.
void Slice::schedule_next()
{
#ifdef VERBOSE
    cerr << __PRETTY_FUNCTION__ << endl;
#endif

    timer.stop();
    current_proc->freeze();
    current_proc->mark_completed();

    if (sc && (!sc->is_empty() && !sc->is_completed() && sc->move_to_next_unfinished_proc()))
        current_proc = &sc->get_current_proc();
    else if (be && (!be->is_empty() && !be->is_completed() && be->move_to_next_unfinished_proc()))
        current_proc = &be->get_current_proc();
    else
        return;

    current_proc->unfreeze();
    timeout += current_proc->get_actual_budget();
    timer.start(timeout);
}

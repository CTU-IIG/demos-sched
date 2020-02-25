#include "slice.hpp"

using namespace std;

Slice::Slice(ev::loop_ref loop,
             std::chrono::steady_clock::time_point start_time,
             Partition &sc,
             Partition &be,
             std::string cpus)
    : sc(sc)
    , be(be)
    , cpus(cpus)
    , timeout(start_time)
    , timer(loop)
{
    sc.bind_empty_cb(std::bind(&Slice::empty_partition_cb, this));
    be.bind_empty_cb(std::bind(&Slice::empty_partition_cb, this));
    timer.set(std::bind(&Slice::timeout_cb, this));
    empty = false;
}

void Slice::bind_empty_cb(std::function<void()> new_empty_cb)
{
    empty_cb = new_empty_cb;
}

void Slice::move_proc_and_start_timer(Partition &p)
{
    p.move_to_next_unfinished_proc();
    timeout += p.get_current_proc().get_actual_budget();
    timer.start(timeout);
}

void Slice::empty_partition_cb()
{
    if (sc.is_empty() && be.is_empty()) {
        cerr << __PRETTY_FUNCTION__ << endl;
        empty = true;
        if (empty_cb)
            empty_cb();
    }
}

void Slice::start()
{
    sc.clear_completed_flag();
    be.clear_completed_flag();

    sc.set_cpus(cpus);
    be.set_cpus(cpus);

    if (!sc.is_empty()) {
        sc.move_to_first_proc();
        current_proc = &sc.get_current_proc();
        current_proc->unfreeze();
        timeout += current_proc->get_actual_budget();
        timer.start(timeout);
    } else if (!be.is_empty()) {
        be.move_to_first_proc();
        current_proc = &be.get_current_proc();
        current_proc->unfreeze();
        timeout += current_proc->get_actual_budget();
        timer.start(timeout);
    }
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

void Slice::update_timeout(std::chrono::steady_clock::time_point actual_time)
{
    timeout = actual_time;
}

void Slice::timeout_cb()
{
#ifdef VERBOSE
    std::cerr << __PRETTY_FUNCTION__ << std::endl;
#endif

    current_proc->freeze();
    current_proc->mark_completed();

    if (!sc.is_empty() && !sc.is_completed() && sc.move_to_next_unfinished_proc()) {
        current_proc = &sc.get_current_proc();
        current_proc->unfreeze();
        timeout += current_proc->get_actual_budget();
        timer.start(timeout);
        return;
    } else if (!be.is_empty() && !be.is_completed() && be.move_to_next_unfinished_proc()) {
        current_proc = &be.get_current_proc();
        current_proc->unfreeze();
        timeout += current_proc->get_actual_budget();
        timer.start(timeout);
        return;
    }

    timer.stop();
}

#include "slice.hpp"

Slice::Slice(ev::loop_ref loop, Partition &sc, Partition &be, Cpu cpus)
    : sc(sc)
    , be(be)
    , cpus(cpus)
    , timeout( start_time )
    , timer(loop)
{
    timer.set(std::bind(&Slice::timeout_cb, this));
    // timer.start(start_time + timeout);
}

void Slice::move_proc_and_start_timer( Partition &p )
{
    p.get_current_proc().freeze();
    p.move_to_next_unfinished_proc();
    p.get_current_proc().unfreeze();
    timeout += p.get_current_proc().get_actual_budget();
    timer.start(timeout);
}

void Slice::start()
{
    for(Process &p : sc.processes)
        p.mark_uncompleted();
    for(Process &p : be.processes)
        p.mark_uncompleted();

    if( !sc.is_empty() ){
        sc.move_to_next_unfinished_proc();
        current_proc = &sc.get_current_proc();
        current_proc->unfreeze();
        timeout += current_proc->get_actual_budget();
        timer.start(timeout);
    } else if ( !be.is_empty() ) {
        be.move_to_next_unfinished_proc();
        current_proc = &sc.get_current_proc();
        current_proc->unfreeze();
        timeout += current_proc->get_actual_budget();
        timer.start(timeout);
    }
}

void Slice::stop()
{
    if( !sc.is_empty() ){
        current_proc->freeze();
    } else if ( !be.is_empty() ) {
        current_proc->freeze();
    }
}

void Slice::update_timeout(std::chrono::steady_clock::time_point actual_time)
{
    timeout = actual_time;
}

void Slice::timeout_cb()
{
    std::cerr << __PRETTY_FUNCTION__ << std::endl;
    std::cerr << "current proc " << current_proc << std::endl;

    current_proc->freeze();
    current_proc->mark_completed();

    //std::cerr<< sc.is_done() <<std::endl;

    if( !sc.is_empty() && !sc.is_done() && sc.move_to_next_unfinished_proc() ){
        current_proc = &sc.get_current_proc();
        current_proc->unfreeze();
        timeout += current_proc->get_actual_budget();
        timer.start(timeout);
        return;
    } else if( !be.is_empty() && !be.is_done() && be.move_to_next_unfinished_proc() ) {
        current_proc = &be.get_current_proc();
        current_proc->unfreeze();
        timeout += current_proc->get_actual_budget();
        timer.start(timeout);
        return;
    }

    stop();
}

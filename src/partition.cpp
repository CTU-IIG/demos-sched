#include "partition.hpp"
#include "lib/assert.hpp"
#include <algorithm>

using namespace std;

Partition::Partition(Cgroup &freezer_parent,
                     Cgroup &cpuset_parent,
                     Cgroup &events_parent,
                     const string &name)
    : name(name)
    , current_proc(nullptr)
    , cgc(cpuset_parent, name)
    , cgf(freezer_parent, name)
    , cge(events_parent, name)
{}

void Partition::kill_all()
{
    for (auto &p : processes) {
        p.kill();
    }
}

void Partition::add_process(ev::loop_ref loop,
                            const string &argv,
                            const optional<filesystem::path> &working_dir,
                            chrono::milliseconds budget,
                            chrono::milliseconds budget_jitter,
                            bool has_initialization)
{
    processes.emplace_back(loop,
                           "proc" + to_string(proc_count),
                           *this,
                           argv,
                           working_dir,
                           budget,
                           budget_jitter,
                           has_initialization);
    proc_count++;
    current_proc = processes.begin();
    empty = false;
}

void Partition::create_processes()
{
    for (auto &p : processes) {
        p.exec();
        cgc.add_process(p.get_pid());
    }
}

void Partition::reset(bool move_to_first_proc,
                      const cpu_set &cpus,
                      const function<void(Process &)> &process_completion_cb)
{
    // must be non-empty
    // MK: this is not due to technical reasons, but I don't have any use-case
    //  for the callback being empty, and enforcing this might avoid some bugs;
    //  feel free to change it if you need to do so
    ASSERT(process_completion_cb);

    clear_completed_flag();
    cgc.set_cpus(cpus);
    _completed_cb = process_completion_cb;
    // for BE partition, we don't want to reset to first process
    if (move_to_first_proc) {
        current_proc = processes.begin();
    }
}

void Partition::disconnect()
{
    _completed_cb = [](Process &) {};
}

void Partition::completed_cb(Process &completed_process)
{
    _completed_cb(completed_process);
}

// cyclic queue
void Partition::move_to_next_proc()
{
    if (++current_proc == processes.end()) {
        current_proc = processes.begin();
    }
}

Process *Partition::seek_pending_process()
{
    for (size_t i = 0; i < processes.size(); i++) {
        if (current_proc->is_pending()) return &*current_proc;
        move_to_next_proc();
    }
    // if no process is left pending, we've executed all processes during this window
    // moving to next process means that in the next window, execution will continue
    //  round the circular queue instead of repeating the last finished process again
    move_to_next_proc();
    return nullptr;
}

void Partition::clear_completed_flag()
{
    for (auto &p : processes) {
        p.mark_uncompleted();
    }
}

string Partition::get_name() const
{
    return name;
}

bool Partition::is_empty() const
{
    return empty;
}

void Partition::set_process_exit_cb(ExitCb new_exit_cb)
{
    _proc_exit_cb = move(new_exit_cb);
}

void Partition::proc_exit_cb(Process &proc)
{
    empty = true;
    for (auto &p : processes) {
        if (p.is_spawned()) empty = false;
    }
    // notify that a process exited
    _proc_exit_cb(proc, empty);
}

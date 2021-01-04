#include "partition.hpp"
#include "log.hpp"
#include <algorithm>
#include <string.h>

using namespace std::placeholders;
using namespace std;

Partition::Partition(Cgroup &freezer_parent,
                     Cgroup &cpuset_parent,
                     Cgroup &events_parent,
                     std::string name)
    : cgc(cpuset_parent, name)
    , cgf(freezer_parent, name)
    , cge(events_parent, name)
    , current_proc(nullptr)
    , name(name)
{
    freeze();
}

Process &Partition::get_current_proc()
{
    return *current_proc;
}

void Partition::freeze()
{
    for (auto &p : processes) {
        p.freeze();
    }
}

void Partition::unfreeze()
{
    for (auto &p : processes) {
        p.unfreeze();
    }
}

void Partition::kill_all()
{
    for (auto &p : processes) {
        p.kill();
    }
}

void Partition::add_process(ev::loop_ref loop,
                            string argv,
                            chrono::nanoseconds budget,
                            bool has_initialization)
{
    processes.emplace_back(
      loop, "proc" + to_string(proc_count), *this, argv, budget, has_initialization);
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
                      const cpu_set cpus,
                      std::function<void()> process_completion_cb)
{
    // must be non-null
    assert(process_completion_cb);

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
    _completed_cb = default_completed_cb;
}

void Partition::completed_cb()
{
    _completed_cb();
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
    if (finished || empty) {
        return nullptr;
    }
    for (size_t i = 0; i < processes.size(); i++) {
        if (current_proc->is_pending()) return &*current_proc;
        move_to_next_proc();
    }
    finished = true;
    return nullptr;
}

void Partition::clear_completed_flag()
{
    finished = false;
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

void Partition::set_empty_cb(std::function<void()> new_empty_cb)
{
    empty_cb = new_empty_cb;
}

void Partition::proc_exit_cb()
{
    for (auto &p : processes) {
        if (p.is_running()) return;
    }
    empty = true;
    // notify that there are no running processes in this partition
    empty_cb();
}

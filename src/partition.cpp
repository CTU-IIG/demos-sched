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

void Partition::add_process(ev::loop_ref loop,
                            string argv,
                            chrono::nanoseconds budget,
                            bool has_initialization)
{
    processes.emplace_back(loop, "proc" + to_string(proc_count), *this, argv, budget);
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
    clear_completed_flag();
    cgc.set_cpus(cpus);
    // if passed callback is empty (nullptr), set default callback
    completed_cb = process_completion_cb ? process_completion_cb : default_completed_cb;
    // for BE partition, we don't want to reset to first process
    if (move_to_first_proc) {
        current_proc = processes.begin();
    }
}

void Partition::disconnect()
{
    completed_cb = default_completed_cb;
}

// cyclic queue
void Partition::move_to_next_proc()
{
    if (++current_proc == processes.end()) {
        current_proc = processes.begin();
    }
}

Process *Partition::find_unfinished_process()
{
    if (completed || empty) {
        return nullptr;
    }
    for (size_t i = 0; i < processes.size(); i++) {
        if (!current_proc->is_completed()) return &*current_proc;
        move_to_next_proc();
    }
    completed = true;
    return nullptr;
}

void Partition::clear_completed_flag()
{
    completed = false;
    for (auto &p : processes) {
        p.mark_uncompleted();
    }
}

string Partition::get_name() const
{
    return name;
}

bool Partition::is_empty()
{
    return empty;
}

void Partition::kill_all()
{
    for (auto &p : processes) {
        p.kill();
    }
}

void Partition::add_empty_cb(std::function<void()> new_empty_cb)
{
    empty_cbs.push_back(new_empty_cb);
}

void Partition::proc_exit_cb(Process &proc)
{
    logger->debug("Process {} exited (partition '{}')", proc.get_pid(), name);

    // check if there are any running processes in this partition
    for (auto &p : processes) {
        if (p.is_running()) return;
    }

    empty = true;
    // notify all slices which own this partition that there are no running processes
    for (auto &cb : empty_cbs) {
        cb();
    }
}

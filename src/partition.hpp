#pragma once

#include "cgroup.hpp"
#include "check_lib.hpp"
#include "cpu_set.hpp"
#include "process.hpp"
#include <chrono>
#include <functional>
#include <list>

class Partition;

using namespace std::placeholders;
using Partitions = std::list<Partition>;
using Processes = std::list<Process>;

/**
 * Container for a group of processes that are scheduled as a single unit. Only
 * a single process from each partition can run at a time - processes are ran
 * sequentially, starting from the first one (but see below).
 *
 * There are 2 types of partitions (depending on slice configuration):
 * In safety-critical partitions, execution in each window restarts from the first
 * process, even if not all processes had chance to run in the last window;
 * in best-effort partitions, execution is continued from last unfinished process.
 */
class Partition
{
public:
    Partition(Cgroup &freezer_parent,
              Cgroup &cpuset_parent,
              Cgroup &events_parent,
              const std::string &name = "");

    /** Kills all system processes from this partition. */
    void kill_all();

    /**
     * Adds a new process to the partition,
     * but doesn't spawn a matching system process.
     *
     * `create_processes()` must be explicitly called after all processes are added.
     */
    void add_process(ev::loop_ref loop,
                     const std::string &argv,
                     std::chrono::milliseconds budget,
                     bool has_initialization);

    /**
     * Spawns system processes for added Process instances.
     */
    void create_processes();

    /**
     * Prepare partition for running under a new owner (currently either Slice or PartitionManager).
     * After this is called, you may start scheduling processes from this partition.
     *
     * Call `disconnect()` after you are done with this partition and do not wish
     * to receive more completion callbacks.
     *
     * TODO: not really sure about the naming, but `reset` and `disconnect` seem the best;
     *  before, it was `bind_to_slice` and `unbind`, but these are not only used by slices,
     *  but also during init; if you have anything better, feel free to change these
     */
    void reset(bool move_to_first_proc,
               const cpu_set &cpus,
               const std::function<void()> &process_completion_cb);

    /**
     * Clears process completion callback set in `bind_to_slice(...)`.
     */
    void disconnect();

    /**
     * Returns pointer to next pending process, if there is one.
     * If current process is pending, return that one (without seeking to the next).
     * If no process is pending, returns nullptr.
     *
     * Pending process is a process which is both:
     *  1) running (=did not exit yet) and
     *  2) incomplete (=did not signal completion or run out of its budget
     *     since last call to `reset(...)`)
     *  (basically, the next process for which it makes sense to run in current window)
     */
    Process *seek_pending_process();

    /**
     * Registers a callback that is called whenever any process exits.
     *
     * The single boolean parameter to the callback indicates if the partition is empty.
     */
    void set_process_exit_cb(std::function<void(bool)> new_exit_cb);

    /** Returns true if there are no running processes inside this partition. */
    [[nodiscard]] bool is_empty() const;
    [[nodiscard]] std::string get_name() const;

    CgroupCpuset cgc;
    // cgf and cge are read by Process constructor in process.cpp
    CgroupFreezer cgf;
    // does not need to be CgroupEvents, we only use it to create child cgroups for processes
    Cgroup cge;
    void proc_exit_cb();
    void completed_cb();

private:
    Processes processes = {};
    Processes::iterator current_proc;
    std::string name;
    size_t proc_count = 0;

    // when true, there are no pending processes
    // cached flag, not strictly needed, speeds up lookup in `seek_pending_process()`
    bool finished = false;
    bool empty = true;

    // cyclic queue
    void move_to_next_proc();
    void clear_completed_flag();

    // TODO: is this the right place for this definition?
    // cached so that we don't recreate new std::function each time
    inline static const std::function<void()> NOOP = [] {};
    std::function<void()> _completed_cb = NOOP;
    // invoked when a process in this partition exits
    std::function<void(bool)> _proc_exit_cb = [](bool) {};
};

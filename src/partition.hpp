#ifndef PARTITION_HPP
#define PARTITION_HPP

#include "cgroup.hpp"
#include "cpu_set.hpp"
#include "demossched.hpp"
#include "process.hpp"
#include <chrono>
#include <functional>
#include <list>

using namespace std::placeholders;

typedef std::list<Process> Processes;

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
    // Partition( Processes &&processes, std::string cgroup_name );
    Partition(Cgroup &freezer_parent,
              Cgroup &cpuset_parent,
              Cgroup &events_parent,
              std::string name = "");

    Process &get_current_proc();

    /** Resumes all processes in this partition. Idempotent. */
    void unfreeze();
    /** Freezes all processes in this partition. Idempotent. */
    void freeze();
    /** Kills all system processes from this partition. */
    void kill_all();

    /**
     * Adds a new process to the partition,
     * but doesn't spawn a matching system process.
     *
     * `create_processes()` must be explicitly called after all processes are added.
     */
    void add_process(ev::loop_ref loop,
                     std::string argv,
                     std::chrono::nanoseconds budget,
                     bool has_initialization);

    /**
     * Spawns system processes for added Process instances.
     */
    void create_processes();

    /**
     * Prepare partition for running under new slice.
     * After this is called, slice may start scheduling processes from this partition.
     *
     * TODO: not really sure about the naming, but `reset` and `disconnect` seem the best;
     *  before, it was `bind_to_slice` and `unbind`, but these are not only used by slices,
     *  but also during init; if you have anything better, feel free to change these
     */
    void reset(bool move_to_first_proc,
               const cpu_set cpus,
               std::function<void()> process_completion_cb);

    /**
     * Clears process completion callback set in `bind_to_slice(...)`.
     */
    void disconnect();

    /**
     * Returns pointer to next unfinished process,
     * if there is one, otherwise returns nullptr.
     */
    Process *find_unfinished_process();

    /** Registers a callback that is called when the partition is emptied (all processes ended). */
    void add_empty_cb(std::function<void()> new_empty_cb);

    /** Returns true if there are no running processes inside this partition. */
    bool is_empty() const;
    std::string get_name() const;

    // protected:
    CgroupCpuset cgc;
    // cgf and cge are read by Process constructor in process.cpp
    CgroupFreezer cgf;
    Cgroup cge;
    void proc_exit_cb(Process &proc);
    void completed_cb();

private:
    Processes processes = {};
    Processes::iterator current_proc;
    std::string name;
    size_t proc_count = 0;

    bool completed = false;
    bool empty = true;

    // cyclic queue
    void move_to_next_proc();
    void clear_completed_flag();

    // cached so that we don't recreate new std::function each time
    std::function<void()> default_completed_cb = [] {};
    std::function<void()> _completed_cb = default_completed_cb;

    // callbacks invoked when partition is empty (no running processes)
    std::vector<std::function<void()>> empty_cbs = {};
};

#endif // PARTITION_HPP

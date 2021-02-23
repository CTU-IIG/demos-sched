#pragma once

#include "log.hpp"
#include "majorframe.hpp"
#include "slice.hpp"

class PartitionManager
{
public:
    PartitionManager(Partitions &partitions)
        : partitions(partitions)
    {
        set_exit_cb(&PartitionManager::process_exit_cb);
    }

    // delete move and copy constructors, as we have a pointer data member
    PartitionManager(const PartitionManager &) = delete;
    const PartitionManager &operator=(const PartitionManager &) = delete;

    /**
     * Initializes all processes.
     *
     * NOTE: initialization may get interrupted if scheduler receives SIGINT or SIGTERM signal.
     */
    void run_process_init(MajorFrame &mf, std::function<void()> init_cb)
    {
        logger->debug("Process initialization started");
        this->init_cb = init_cb;

        // used when partition is not contained in any slice, not really important
        const cpu_set default_cpu_set{};
        const cpu_set *cpus_ptr;
        auto completion_cb = std::bind(&PartitionManager::process_init_completion_cb, this);
        for (auto &p : partitions) {
            // we want to run init for each partition in the widest
            //  cpu_set it will ever run in; otherwise, multi-threaded
            //  program could initialize to run with lower number of threads,
            //  which would be inefficient; this way, the process might run
            //  on fewer cores than it has threads in some windows, but that
            //  isn't as much of a problem for performance
            cpus_ptr = mf.find_widest_cpu_set(p);
            p.reset(true, cpus_ptr ? *cpus_ptr : default_cpu_set, completion_cb);
        }

        // overwrite process exit callback set in constructor
        // we need this to be able to detect that a process exited during initialization
        //  and we should move on to the next one, otherwise a deadlock would occur
        // this will be reset to the original callback after initialization is finished
        set_exit_cb(&PartitionManager::process_init_exit_cb);

        init_next_process();
    }

    /** Spawns suspended processes from all partitions. */
    void create_processes()
    {
        for (auto &p : partitions) {
            p.create_processes();
        }
    }

    /**
     * Kills all underlying system processes.
     *
     * Processes might not terminate immediately, but eventual termination is guaranteed.
     * To get notified when that happens, set a callback by calling `set_completion_cb(...)`.
     */
    void kill_all()
    {
        for (auto &p : partitions) {
            p.kill_all();
        }
    }

    /**
     * Sets callback that is called when all partitions
     * are empty (so there are no processes to schedule).
     */
    void set_completion_cb(std::function<void()> completion_cb)
    {
        this->completion_cb = completion_cb;
    }

private:
    Partitions &partitions;
    Partitions::iterator init_iter = partitions.begin();
    Process *initialized_proc = nullptr;
    std::function<void()> completion_cb = [] {};
    // initialized in `run_process_init`
    std::function<void()> init_cb = nullptr;

    /** Sets up passed process exit callback, bound to `this`. */
    void set_exit_cb(void (PartitionManager::* cb_method)(bool))
    {
        using namespace std::placeholders;
        auto proc_exit_cb = std::bind(cb_method, this, _1);
        for (auto &p : partitions) {
            p.set_process_exit_cb(proc_exit_cb);
        }
    }

    /** Called when a process exits in one of the managed partitions. */
    void process_exit_cb(bool partition_empty)
    {
        if (!partition_empty) return;
        for (auto &p : partitions) {
            if (!p.is_empty()) return;
        }
        completion_cb();
    }

    Process *find_next_uninitialized_process()
    {
        Process *proc;
        // find next uninitialized process
        for (; init_iter != partitions.end(); init_iter++) {
            proc = init_iter->seek_pending_process();
            if (proc) return proc;
        }
        return nullptr;
    }

    void init_next_process()
    {
        initialized_proc = find_next_uninitialized_process();
        if (initialized_proc) {
            init_process(initialized_proc);
        } else {
            finish_process_init();
        }
    }

    /**
     * Initializes a single process.
     * Completion is handled by `process_init_completion_cb()`.
     */
    void init_process(Process *p)
    {
        if (!p->needs_initialization()) {
            p->mark_completed();
            init_next_process();
            return;
        }

        logger->trace("Initializing process '{}'", p->get_pid());
        // resume the process; it should stop on its own after initialization is completed
        // then, it will call init_next_process as callback
        p->resume();
    }

    void finish_process_init()
    {
        // reset process exit cb to the original one
        set_exit_cb(&PartitionManager::process_exit_cb);
        // remove completion cb
        for (Partition &p : partitions) {
            p.disconnect();
        }
        logger->debug("Process initialization finished");
        init_cb();
    }

    /** Called as a completion callback from process. */
    void process_init_completion_cb()
    {
        initialized_proc->suspend();
        initialized_proc->mark_completed();
        init_next_process();
    }

    /** Called when a process exits during initialization. */
    void process_init_exit_cb(bool partition_empty)
    {
        // if this is false, it means some other process exited,
        //  this may happen for example on receiving SIGINT
        if (!initialized_proc->is_running()) {
            // end initialization of this process and move on to the next one
            process_init_completion_cb();
        }

        // call original callback
        process_exit_cb(partition_empty);
    }
};
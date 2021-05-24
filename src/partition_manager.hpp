#pragma once

#include <utility>

#include "log.hpp"
#include "majorframe.hpp"
#include "slice.hpp"

class PartitionManager
{
private:
    Partitions partitions;
    std::function<void()> completion_cb = [] {};
    // initialized in `run_process_init`
    std::function<void()> init_cb = nullptr;
    int remaining_process_inits = 0;

public:
    explicit PartitionManager(Partitions &&partitions)
        : partitions(std::move(partitions))
    {
        set_exit_cb(&PartitionManager::process_exit_cb);
    }

    // delete copy and (implicitly) move constructors
    PartitionManager(const PartitionManager &) = delete;
    const PartitionManager &operator=(const PartitionManager &) = delete;

    /**
     * Initializes all processes.
     *
     * NOTE: initialization may get interrupted if scheduler receives SIGINT or SIGTERM signal.
     */
    void run_process_init(MajorFrame &mf, std::function<void()> init_cb_)
    {
        logger->debug("Starting process initialization");
        init_cb = std::move(init_cb_);

        // used when partition is not contained in any slice, not really important
        const cpu_set default_cpu_set{};
        const cpu_set *cpus_ptr;
        auto part_completion_cb = [this](Process &p) { process_init_completion_cb(p); };

        for (auto &p : partitions) {
            // we want to run init for each partition in the widest
            //  cpu_set it will ever run in; otherwise, multi-threaded
            //  program could initialize to run with lower number of threads,
            //  which would be inefficient; this way, the process might run
            //  on fewer cores than it has threads in some windows, but that
            //  isn't as much of a problem for performance
            cpus_ptr = mf.find_widest_cpu_set(p);
            p.reset(true, cpus_ptr ? *cpus_ptr : default_cpu_set, part_completion_cb);
        }

        // overwrite process exit callback set in constructor
        // we need this to be able to detect that a process exited during initialization,
        //  otherwise a deadlock would occur, waiting for the completion of an exited process
        // this will be reset to the original callback after initialization is finished
        set_exit_cb(&PartitionManager::process_init_exit_cb);

        // count and start all processes that needs initialization in parallel
        for (auto &part : partitions) {
            for (auto &proc : part.processes) {
                if (!proc.needs_initialization()) continue;
                logger->trace("Initializing process '{}'", proc.get_pid());
                remaining_process_inits++;
                // resume the process; it should stop on its own after initialization is completed
                // then, it will call proc_init_completion_cb as callback; if it exits instead
                // (probably due to an error), `process_init_exit_cb` will be called
                proc.resume();
            }
        }

        if (remaining_process_inits == 0) {
            logger->info("No processes need initialization");
            // no process needs initialization, we're done
            finish_process_init();
        } else {
            logger->info("Initializing {} {}",
                         remaining_process_inits,
                         remaining_process_inits == 1 ? "process" : "processes");
        }
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
    void set_completion_cb(std::function<void()> completion_cb_)
    {
        this->completion_cb = std::move(completion_cb_);
    }

private:
    /** Sets up passed process exit callback, bound to `this`. */
    void set_exit_cb(void (PartitionManager::*cb_method)(Process &, bool))
    {
        using namespace std::placeholders;
        auto proc_exit_cb = std::bind(cb_method, this, _1, _2); // NOLINT(modernize-avoid-bind)
        for (auto &p : partitions) {
            p.set_process_exit_cb(proc_exit_cb);
        }
    }

    /** Called when a process exits in one of the managed partitions. */
    void process_exit_cb(Process &, bool partition_empty)
    {
        if (!partition_empty) return;
        for (auto &p : partitions) {
            if (!p.is_empty()) return;
        }
        completion_cb();
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
    void process_init_completion_cb(Process &proc)
    {
        proc.suspend();
        remaining_process_inits--;
        if (remaining_process_inits == 0) {
            finish_process_init();
        }
    }

    /** Called when a process exits during initialization. */
    void process_init_exit_cb(Process &proc, bool partition_empty)
    {
        if (proc.needs_initialization()) {
            // consider this process initialized
            process_init_completion_cb(proc);
        }
        // call original callback
        process_exit_cb(proc, partition_empty);
    }
};

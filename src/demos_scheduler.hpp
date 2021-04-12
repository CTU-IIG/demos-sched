#pragma once

#include "log.hpp"
#include "majorframe.hpp"
#include "memory_tracker.hpp"
#include "partition_manager.hpp"
#include "slice.hpp"
#include <chrono>
#include <ev++.h>

class DemosScheduler
{
private:
    ev::loop_ref loop;
    MajorFrame mf;
    PartitionManager partition_manager;
    ev::sig sigint{ loop };
    ev::sig sigterm{ loop };

public:
    DemosScheduler(ev::loop_ref ev_loop, Partitions &&partitions, Windows &&windows, const std::string &frame_sync_message)
        : loop(ev_loop)
        , mf(loop, std::move(windows), frame_sync_message)
        , partition_manager(std::move(partitions))
    {
        // setup completion callback
        partition_manager.set_completion_cb([this] { completion_cb(); });
        // setup signal handlers
        sigint.set<DemosScheduler, &DemosScheduler::signal_cb>(this);
        sigterm.set<DemosScheduler, &DemosScheduler::signal_cb>(this);
    }

    /** Sets up scheduler for running. Currently only spawns system processes. */
    void setup() { partition_manager.create_processes(); }

    /**
     * Run demos scheduler in the event loop passed in constructor.
     * Returns either after all processes exit, or SIGTERM/SIGINT is received.
     *
     * It is guaranteed that all processes are ended before this function returns.
     *
     * Assumes that system processes are already created.
     */
    void run()
    {
        // we'll create a zero-length timer and run startup_fn in it
        // that way, the function runs "inside" the event loop
        //  and there isn't a special case for the first function (startup_fn)
        //  (otherwise, the first process would be started before the event loop begins)
        loop.once<DemosScheduler, &DemosScheduler::startup_fn>(-1, 0, 0, this);

        // start signal handlers
        sigint.start(SIGINT);
        sigterm.start(SIGTERM);

        logger->debug("Starting event loop");
        loop.run();
        logger->debug("Event loop stopped");
    }

private:
    void startup_fn()
    {
        partition_manager.run_process_init(mf, [this] { start_scheduler(); });
    }

    void start_scheduler()
    {
        logger->debug("Starting scheduler");
        enable_allocation_logging();
        mf.start(std::chrono::steady_clock::now());
    }

    /**
     * Called when all processes exited (or were terminated).
     *
     * NOTE: this may get called before `start_scheduler()`,
     *  in case all processes exit during initialization.
     */
    void completion_cb()
    {
        logger->debug("All processes exited");
        // there might be some interesting events pending,
        //  we'll use another zero-length timer to let
        //  ev process them before breaking the event loop
        loop.once<DemosScheduler, &DemosScheduler::stop_scheduler>(-1, 0, 0, this);
    }

    void stop_scheduler()
    {
        mf.stop(std::chrono::steady_clock::now());
        loop.break_loop(ev::ALL);
    }

    /** Called when our process receives a stop signal. */
    void signal_cb()
    {
        logger->info("Received stop signal (SIGTERM or SIGINT), stopping all processes");
        disable_allocation_logging();
        // this should trigger completion_cb() when scheduler is running,
        //  and at the same time allows for graceful async cleanup
        partition_manager.kill_all();
    }
};

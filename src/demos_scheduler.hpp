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
    std::optional<std::chrono::milliseconds> timeout{};
    ev::timerfd timeout_timer{ loop };

public:
    DemosScheduler(ev::loop_ref ev_loop,
                   Partitions &&partitions,
                   Windows &&windows,
                   const std::string &window_sync_message,
                   const std::string &mf_sync_message)
        : loop(ev_loop)
        , mf(loop, std::move(windows), window_sync_message, mf_sync_message)
        , partition_manager(std::move(partitions))
    {
        // setup completion callback
        partition_manager.set_completion_cb([this] { completion_cb(); });
        // setup signal handlers
        sigint.set<DemosScheduler, &DemosScheduler::signal_cb>(this);
        sigterm.set<DemosScheduler, &DemosScheduler::signal_cb>(this);
        timeout_timer.set([this] { timeout_cb(); });
    }

    /** Sets up scheduler for running. Currently only spawns system processes. */
    void setup() { partition_manager.create_processes(); }

    /**
     * Runs the scheduler in the event loop passed in constructor.
     * Returns either after all processes exit, or SIGTERM/SIGINT is received.
     *
     * It is guaranteed that all processes are ended before this function returns.
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

    /**
     * Runs the scheduler, stopping after `timeout`,
     * unless stopped earlier in another way (see `run()`).
     */
    void run(std::chrono::milliseconds timeout_)
    {
        timeout = timeout_;
        run();
    }

    /** Asynchronously terminates all scheduled processes and stops the scheduler. */
    void initiate_shutdown()
    {
        // do not log anything, as we don't know the reason why this was called;
        //  let the caller log the reason

        // stop window scheduling, otherwise it would interfere with process termination
        //  (`kill_all` call below unfreezes all processes, and if scheduler was running,
        //  they would get frozen again at the end of the window, potentially before exiting)
        mf.stop(std::chrono::steady_clock::now());

        // this should trigger completion_cb() when scheduler is running,
        //  and at the same time allows for graceful async cleanup
        partition_manager.kill_all();
    }

private:
    void startup_fn()
    {
        partition_manager.run_process_init(mf, [this] { start_scheduler(); });
    }

    void start_scheduler()
    {
        if (timeout && timeout.value() == timeout->zero()) {
            // useful for measuring startup time
            logger->info("Zero timeout set, stopping immediately");
            initiate_shutdown();
            return;
        }

        logger->info("Starting scheduler");
        memory_tracker::enable();
        auto start_time = std::chrono::steady_clock::now();
        Slice::start_time = start_time;
        mf.start(start_time);
        if (timeout) {
            logger->debug("Scheduler will timeout in '{} ms'", timeout->count());
            timeout_timer.start(start_time + timeout.value());
            timeout = std::nullopt;
        }
    }

    /**
     * Called when all processes exited (or were terminated).
     *
     * NOTE: this may get called before `start_scheduler()`,
     *  in case all processes exit during initialization.
     *
     * TODO: "completion" is confusing (with yielding), find better name
     */
    void completion_cb()
    {
        logger->info("All processes exited, stopping scheduler");
        memory_tracker::disable();
        // stop the scheduler
        mf.stop(std::chrono::steady_clock::now());
        // stop the event loop; ev automatically handles all pending events before stopping
        loop.break_loop(ev::ALL);
    }

    /** Called when our process receives a stop signal. */
    void signal_cb()
    {
        logger->info("Received stop signal (SIGTERM or SIGINT), stopping all processes");
        initiate_shutdown();
    }

    /** Called on expiration of the timeout configured by the user. Stops the scheduler. */
    void timeout_cb()
    {
        logger->info("Timed out, stopping all processes");
        initiate_shutdown();
    }
};

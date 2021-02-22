#include "log.hpp"
#include "majorframe.hpp"
#include "partition_manager.hpp"
#include "slice.hpp"
#include <chrono>
#include <ev++.h>

class DemosScheduler
{
public:
    DemosScheduler(ev::loop_ref loop, Partitions &partitions, MajorFrame &mf)
        : loop(loop)
        , mf(mf)
        , partition_manager(partitions)
    {
        // setup completion callback
        partition_manager.set_completion_cb(std::bind(&DemosScheduler::completion_cb, this));
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
    ev::loop_ref loop;
    MajorFrame &mf;
    PartitionManager partition_manager;
    ev::sig sigint{ loop };
    ev::sig sigterm{ loop };

    void startup_fn()
    {
        partition_manager.run_process_init(mf, std::bind(&DemosScheduler::start_scheduler, this));
    }

    void start_scheduler()
    {
        logger->debug("Starting scheduler");
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
        mf.stop();
        loop.break_loop(ev::ALL);
    }

    /** Called when our process receives a stop signal. */
    void signal_cb()
    {
        logger->info("Received stop signal (SIGTERM or SIGINT), stopping all processes");
        // this should trigger completion_cb() when scheduler is running,
        //  and at the same time allows for graceful async cleanup
        partition_manager.kill_all();
    }
};
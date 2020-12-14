#include "log.hpp"
#include "majorframe.hpp"
#include "process_initializer.hpp"
#include "slice.hpp"
#include <chrono>
#include <ev++.h>

class DemosScheduler
{
public:
    DemosScheduler(ev::loop_ref loop, Partitions &partitions, MajorFrame &mf)
        : loop(loop)
        , mf(mf)
        , initializer(partitions)
    {
        // setup completion callback
        mf.set_completed_cb([&] { loop.break_loop(ev::ALL); });
        // setup signal handlers
        sigint.set<DemosScheduler, &DemosScheduler::signal_cb>(this);
        sigterm.set<DemosScheduler, &DemosScheduler::signal_cb>(this);
    }

    /**
     * Run demos scheduler in the event loop passed in constructor.
     * Returns either after all processes exit, or SIGTERM/SIGINT is received.
     *
     * Assumes that system processes are already created.
     *
     * @param start_fn - function that kicks off the
     */
    void run()
    {
        // we create a zero-length timer and run startup_fn in it
        // that way, the function runs "inside" the event loop
        //  and there isn't a special case for the first function (startup_fn)
        //  (otherwise, the first process would be started before the event loop begins)
        ev::timer immediate{ loop };
        immediate.set<DemosScheduler, &DemosScheduler::startup_fn>(this);
        immediate.set(0., 0.);
        immediate.start();

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
    ProcessInitializer initializer;
    ev::sig sigint{ loop };
    ev::sig sigterm{ loop };

    void startup_fn()
    {
        initializer.run_process_init(std::bind(&DemosScheduler::start_scheduler, this));
    }

    void start_scheduler() {
        logger->debug("Starting scheduler");
        mf.start(std::chrono::steady_clock::now());
    }

    void signal_cb()
    {
        logger->info("Received stop signal (SIGTERM or SIGINT), stopping");
        mf.stop();
        loop.break_loop(ev::ALL);
    }
};
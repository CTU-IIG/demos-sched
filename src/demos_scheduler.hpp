#include "majorframe.hpp"
#include <ev++.h>

#include "log.hpp"

class DemosScheduler
{
public:
    DemosScheduler(ev::loop_ref loop, MajorFrame &mf)
        : loop(loop)
        , mf(mf)
    {
        mf.set_completed_cb([&] { loop.break_loop(ev::ALL); });

        // setup signal handlers
        sigint.set<DemosScheduler, &DemosScheduler::signal_cb>(this);
        sigterm.set<DemosScheduler, &DemosScheduler::signal_cb>(this);
        sigint.start(SIGINT);
        sigterm.start(SIGTERM);
    }

    void run(std::function<void()> start_fn)
    {
        this->start_fn = start_fn;

        // we create a zero-length timer and run start_fn in it
        // that way, the function runs "inside" the event loop
        // and there isn't a special case for the first function (start_fn)
        // (otherwise, the first process was started before the event loop began)
        ev::timer immediate{ loop };
        immediate.set<DemosScheduler, &DemosScheduler::immediate_cb>(this);
        immediate.set(0., 0.);
        immediate.start();

        logger->debug("Starting event loop");
        loop.run();
        logger->debug("Event loop stopped");
    }

private:
    ev::loop_ref loop;
    MajorFrame &mf;
    ev::sig sigint{ loop };
    ev::sig sigterm{ loop };
    std::function<void()> start_fn = [] {};

    void immediate_cb() { start_fn(); }

    void signal_cb()
    {
        logger->info("Received stop signal (SIGTERM or SIGINT), stopping");
        mf.stop();
        loop.break_loop(ev::ALL);
    }
};
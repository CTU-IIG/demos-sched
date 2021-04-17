#pragma once

class Window;

/**
 * Interface class that receives events from other parts of the scheduler and reacts to them.
 *
 * This class simplifies event propagation and handling and centralizes it into a single class.
 * The events are (among others) used for modular CPU frequency management.
 *
 * TODO: MK: the name isn't too great, but I can't figure out anything clearer
 *  (EventHandler sounds too much like a one-off event callback in a parameter)
 */
class SchedulerEvents
{
public:
    virtual ~SchedulerEvents() = default;

    virtual void on_window_start(Window &) = 0;
    virtual void on_sc_start(Window &) = 0;
    virtual void on_be_start(Window &) = 0;
    virtual void on_window_end(Window &) = 0;
};

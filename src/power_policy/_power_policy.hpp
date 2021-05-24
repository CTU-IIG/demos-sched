#pragma once

#include <memory>

class Window;

/**
 * A virtual interface, which is injected to other components of the scheduler,
 * and changes CPU power states according to the received events.
 *
 * This class simplifies the event propagation and handling, and centralizes it
 * into a single class. The components call the event handlers directly (through vtable),
 * which minimizes the performance impact.
 */
class PowerPolicy
{
public:
    virtual ~PowerPolicy() = default;

    virtual void on_window_start(Window &) {}
    virtual void on_sc_start(Window &) {}
    virtual void on_be_start(Window &) {}
    virtual void on_window_end(Window &) {}

    // it seems a bit weird to have what's essentially an input parsing function here,
    //  but imo it is still cleaner than including all defined policies in main.cpp
    /** Instantiates a power policy by the name. */
    static std::unique_ptr<PowerPolicy> setup_power_policy(std::string policy_name);
};

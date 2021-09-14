#pragma once

#include <memory>
#include <power_manager.hpp>
#include <process.hpp>
#include <window.hpp>

/**
 * A virtual interface, which is injected to other components of the scheduler,
 * and changes CPU power states according to the received events.
 *
 * This class simplifies the event propagation and handling, and centralizes it
 * into a single class. The components call the event handlers directly (through vtable),
 * which minimizes the performance impact.
 *
 * NOTE: file name is prefixed with '_', to distinguish it from the actual
 *  implementations, which are in the same directory
 */
class PowerPolicy
{
public:
    virtual ~PowerPolicy() = default;

    virtual void validate(const Windows &) {}
    virtual bool supports_per_process_frequencies() { return false; }
    virtual bool supports_per_slice_frequencies() { return false; }

    virtual void on_window_start(Window &) {}
    virtual void on_sc_start(Window &) {}
    virtual void on_be_start(Window &) {}
    virtual void on_window_end(Window &) {}

    // TODO: check the performance impact, potentially hide it behind a compile-time flag
    virtual void on_process_start(Process &) {}
    virtual void on_process_end(Process &) {}

    // it seems a bit weird to have what's essentially an input parsing function here,
    //  but imo it is still cleaner than including all defined policies in main.cpp
    /**
     * Instantiates a power policy by the name.
     *
     * Policies may accept extra arguments, which are passed in the format
     * `<policy_name>:<arg1>,<arg2>,...`
     */
    static std::unique_ptr<PowerPolicy> setup_power_policy(const std::string &policy_str);
};

/**
 * Subclass of PowerPolicy, which is extended by power policy implementations
 * which use PowerManager. In the rest of the codebase, you should accept
 * the more general PowerPolicy interface instead of this one.
 */
class PmPowerPolicy : public PowerPolicy {
protected:
    PowerManager pm{};
};

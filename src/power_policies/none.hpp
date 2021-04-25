#pragma once

#include "power_policies/power_policy.hpp"
#include "window.hpp"

/**
 * Empty PowerPolicy that does not do anything.
 */
class PowerPolicy_None : public PowerPolicy
{
public:
    void on_sc_start(Window &) override {}
    void on_be_start(Window &) override {}
    void on_window_start(Window &) override {}
    void on_window_end(Window &) override {}
};

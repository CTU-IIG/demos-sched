#pragma once

#include "power_policies/power_policy.hpp"
#include "window.hpp"

class PowerPolicy_MinBE : public PowerPolicy
{
private:
    PowerManager pm{};

public:
    void on_sc_start(Window &) override
    {
        // run SC partitions on max frequency
        for (auto &p : pm.policy_iter()) {
            p.write_frequency(p.max_frequency);
        }
    }

    void on_be_start(Window &) override
    {
        // run BE partitions on min frequency
        for (auto &p : pm.policy_iter()) {
            p.write_frequency(p.min_frequency);
        }
    }

    void on_window_start(Window &) override {}
    void on_window_end(Window &) override {}
};

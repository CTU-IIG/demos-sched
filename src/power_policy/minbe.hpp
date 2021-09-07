#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"

class PowerPolicy_MinBE : public PmPowerPolicy
{
public:
    PowerPolicy_MinBE()
    {
        // run initialization on max frequency
        for (auto &p : pm.policy_iter()) {
            p.write_frequency(p.max_frequency);
        }
    }

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
};

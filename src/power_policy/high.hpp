#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"

class PowerPolicy_FixedHigh : public PmPowerPolicy
{
public:
    PowerPolicy_FixedHigh()
    {
        for (auto &p : pm.policy_iter()) {
            p.write_frequency(p.max_frequency);
        }
    }
};

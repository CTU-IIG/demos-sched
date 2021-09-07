#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"

class PowerPolicy_FixedLow : public PmPowerPolicy
{
public:
    PowerPolicy_FixedLow()
    {
        for (auto &p : pm.policy_iter()) {
            p.write_frequency(p.min_frequency);
        }
    }
};

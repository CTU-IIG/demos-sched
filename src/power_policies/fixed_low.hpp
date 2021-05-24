#pragma once

#include "power_policies/power_policy.hpp"
#include "window.hpp"

class PowerPolicy_FixedLow : public PowerPolicy
{
private:
    PowerManager pm{};

public:
    PowerPolicy_FixedLow()
    {
        for (auto &p : pm.policy_iter()) {
            p.write_frequency(p.min_frequency);
        }
    }
};

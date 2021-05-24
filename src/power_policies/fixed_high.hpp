#pragma once

#include "power_policies/power_policy.hpp"
#include "window.hpp"

class PowerPolicy_FixedHigh : public PowerPolicy
{
private:
    PowerManager pm{};

public:
    PowerPolicy_FixedHigh()
    {
        for (auto &p : pm.policy_iter()) {
            p.write_frequency(p.max_frequency);
        }
    }
};

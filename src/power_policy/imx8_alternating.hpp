#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"

/**
 * Alternates the A53 cluster on the i.MX8 between the 2 passed frequencies.
 * A72 cluster is fixed at the max frequency.
 *
 * NOTE: this is a test power policy, the error messages are not very helpful
 */
class PowerPolicy_Imx8_AlternatingA53 : public PowerPolicy
{
private:
    PowerManager pm{};
    CpufreqPolicy &a53_pol = pm.get_policy("policy0");
    CpufreqPolicy &a72_pol = pm.get_policy("policy4");
    CpuFrequencyHz f1_a53 = 0;
    CpuFrequencyHz f2_a53 = 0;
    bool f1_next = true;

public:
    PowerPolicy_Imx8_AlternatingA53(const std::string &a53_f1_str, const std::string &a53_f2_str)
    {
        for (auto &p : pm.policy_iter()) {
            if (!p.available_frequencies) {
                throw runtime_error("Cannot list available frequencies for the CPU"
                                    " - are you sure you're running DEmOS on an i.MX8?");
            }
        }

        try {
            f1_a53 = a53_pol.available_frequencies->at(std::stoul(a53_f1_str));
            f2_a53 = a53_pol.available_frequencies->at(std::stoul(a53_f2_str));
        } catch (...) {
            throw runtime_error("Both power policy arguments must be integers in the range <0, 3>");
        }

        // fix A72 on max frequency
        a72_pol.write_frequency(a72_pol.max_frequency);
    }

    void on_window_start(Window &) override
    {
        a53_pol.write_frequency(f1_next ? f1_a53 : f2_a53);
        f1_next = !f1_next;
    }
};

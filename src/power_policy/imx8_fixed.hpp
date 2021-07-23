#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"
#include "window.hpp"

/**
 * Sets fixed frequency for both clusters (A53 and A72) on the i.MX8.
 *
 * NOTE: this is a test power policy, the error messages are not very helpful
 */
class PowerPolicy_Imx8_Fixed : public PowerPolicy
{
private:
    PowerManager pm{};

public:
    /** *_freq_i - integer, range <0,3> */
    PowerPolicy_Imx8_Fixed(const std::string& a53_freq_i, const std::string& a72_freq_i)
    {
        auto &a53_pol = pm.get_policy("policy0");
        auto &a72_pol = pm.get_policy("policy4");

        for (auto &p : pm.policy_iter()) {
            if (!p.available_frequencies) {
                throw runtime_error("Cannot list available frequencies for the CPU"
                                    " - are you sure you're running DEmOS on an i.MX8?");
            }
        }

        CpuFrequencyHz a53_freq, a72_freq;
        try {
            a53_freq = a53_pol.get_freq(std::stoul(a53_freq_i));
            a72_freq = a72_pol.get_freq(std::stoul(a72_freq_i));
        } catch (...) {
            throw runtime_error("Both power policy arguments must be integers in the range <0, 3>");
        }

        a53_pol.write_frequency(a53_freq);
        a72_pol.write_frequency(a72_freq);
    }
};

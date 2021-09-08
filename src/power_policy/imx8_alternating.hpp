#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"

/**
 * Alternates A53 and A72 clusters on the i.MX8 between the passed frequencies.
 *
 * NOTE: this is a test power policy, the error messages are not very helpful
 */
class PowerPolicy_Imx8_Alternating : public PmPowerPolicy
{
private:
    CpufreqPolicy &a53_pol = pm.get_policy("policy0");
    CpufreqPolicy &a72_pol = pm.get_policy("policy4");
    CpuFrequencyHz f1_a53;
    CpuFrequencyHz f2_a53;
    CpuFrequencyHz f1_a72;
    CpuFrequencyHz f2_a72;
    bool f1_next = true;

public:
    PowerPolicy_Imx8_Alternating(const std::string &a53_f1_str,
                                 const std::string &a53_f2_str,
                                 const std::string &a72_f1_str,
                                 const std::string &a72_f2_str)
    try
      : PmPowerPolicy{}
      , f1_a53{ a53_pol.get_freq(std::stoul(a53_f1_str)) }
      , f2_a53{ a53_pol.get_freq(std::stoul(a53_f2_str)) }
      , f1_a72{ a72_pol.get_freq(std::stoul(a72_f1_str)) }
      , f2_a72{ a72_pol.get_freq(std::stoul(a72_f2_str)) }
    {
        for (auto &p : pm.policy_iter()) {
            if (!p.available_frequencies) {
                throw runtime_error("Cannot list available frequencies for the CPU"
                                    " - are you sure you're running DEmOS on an i.MX8?");
            }
        }

        // set fixed frequency if both "alternating" frequencies are the same
        if (f1_a53 == f2_a53) a53_pol.write_frequency(f1_a53);
        if (f1_a72 == f2_a72) a72_pol.write_frequency(f1_a72);
    } catch (...) {
        throw_with_nested(
          runtime_error("All power policy arguments must be integers in the range <0, 3>"));
    }

    void on_window_start(Window &) override
    {
        if (f1_a53 != f2_a53) a53_pol.write_frequency(f1_next ? f1_a53 : f2_a53);
        if (f1_a72 != f2_a72) a72_pol.write_frequency(f1_next ? f1_a72 : f2_a72);
        f1_next = !f1_next;
    }
};

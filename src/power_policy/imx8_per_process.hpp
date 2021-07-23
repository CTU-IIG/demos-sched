#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"
#include "window.hpp"

/**
 * Set frequency based on the currently executing process.
 *
 * NOTE: this is a test power policy, the error messages are not very helpful
 */
class PowerPolicy_Imx8_PerProcess : public PowerPolicy
{
private:
    PowerManager pm{};
    CpufreqPolicy &a53_pol;
    CpufreqPolicy &a72_pol;

public:
    PowerPolicy_Imx8_PerProcess()
        : a53_pol{ pm.get_policy("policy0") }
        , a72_pol{ pm.get_policy("policy4") }
    {
        for (auto &p : pm.policy_iter()) {
            if (!p.available_frequencies) {
                throw runtime_error("Cannot list available frequencies for the CPU"
                                    " - are you sure you're running DEmOS on an i.MX8?");
            }
        }
    }

    void on_process_start(Process &proc) override
    {
        // TODO: better bound checking
        if (proc.a53_freq_i) {
            a53_pol.write_frequency(a53_pol.available_frequencies->at(proc.a53_freq_i.value()));
        }
        if (proc.a72_freq_i) {
            a72_pol.write_frequency(a72_pol.available_frequencies->at(proc.a72_freq_i.value()));
        }
    }
};

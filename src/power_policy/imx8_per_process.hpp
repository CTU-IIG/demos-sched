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
    const cpu_set a53_cpus{0b001111};
    const cpu_set a72_cpus{0b110000};

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
        // check if the process is requesting any specific frequency and
        //  if its current cpuset intersects with the A53/A72 cluster
        cpu_set proc_cpus = proc.part.current_cpus;
        if (proc.a53_freq && proc_cpus & a53_cpus) {
            a53_pol.write_frequency(proc.a53_freq.value());
        }
        if (proc.a72_freq && proc_cpus & a72_cpus) {
            a72_pol.write_frequency(proc.a72_freq.value());
        }
    }
};

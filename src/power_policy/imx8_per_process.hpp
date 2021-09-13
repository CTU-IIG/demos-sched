#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"
#include "window.hpp"

/**
 * Set frequency based on the currently executing process.
 *
 * The requested frequencies for processes are validated at runtime, and when a collision
 * occurs (multiple processes request different frequencies for the same CPU cluster), an
 * error message is shown.
 * Originally, an ahead-of-time collision algorithm was used, but it was quite complex,
 * and a clean implementation required refactoring a significant part of the codebase
 * (Config required access to PowerManager), which was deemed too time-intensive
 * to implement for a single experimental feature.
 */
class PowerPolicy_Imx8_PerProcess : public PmPowerPolicy
{
private:
    // currently, this is hardcoded for i.MX8, but should be quite simply extensible
    //  for other CPU cluster layouts
    std::array<CpufreqPolicy *, 2> policies{ &pm.get_policy("policy0"), &pm.get_policy("policy4") };
    // a list of currently running processes which requested a fixed frequency
    // there may be multiple processes which requested the same frequency,
    //  which is why a list is used instead of a single pointer
    std::array<std::vector<Process *>, 2> locking_process_for_policy{};

public:
    PowerPolicy_Imx8_PerProcess()
    {
        ASSERT(policies.size() == locking_process_for_policy.size());
        for (auto &p : pm.policy_iter()) {
            if (!p.available_frequencies) {
                throw runtime_error("Cannot list available frequencies for the CPU"
                                    " - are you sure you're running DEmOS on an i.MX8?");
            }
        }
    }

    void on_process_start(Process &proc) override
    {
        ASSERT(proc.requested_frequencies.size() == policies.size());
        // check if the process is requesting any specific frequency and
        //  if its current cpuset intersects with the A53/A72 cluster
        const cpu_set &proc_cpus = proc.part.current_cpus();

        for (size_t i = 0; i < policies.size(); i++) {
            auto &policy = *policies[i];
            auto &locking_processes = locking_process_for_policy[i];
            auto requested_freq = proc.requested_frequencies[i];

            if (requested_freq && proc_cpus & policy.affected_cores) {
                // process is requesting a fixed frequency, check if it is
                //  compatible with existing locks; all locking processes must have requested the
                //  same frequency, so it suffices to check the first one
                if (!locking_processes.empty() &&
                    locking_processes[0]->requested_frequencies[i] != requested_freq) {
                    throw std::runtime_error(
                      fmt::format("Scheduled processes require different frequencies on the CPU "
                                  "cluster '#{}': '{}', '{}' (process 1: '{}', process 2: '{}')",
                                  i,
                                  locking_processes[0]->requested_frequencies[i].value(),
                                  requested_freq.value(),
                                  locking_processes[0]->argv,
                                  proc.argv));
                }
                locking_processes.push_back(&proc);
                policy.write_frequency(requested_freq.value());
            }
        }
    }

    void on_process_end(Process &process) override
    {
        // remove the process from the list of locking processes
        for (auto &lp : locking_process_for_policy) {
            for (auto it = lp.begin(); it != lp.end(); it++) {
                if (*it == &process) {
                    lp.erase(it);
                    break;
                }
            }
        }
    }
};

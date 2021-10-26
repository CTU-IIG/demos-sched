#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"
#include "window.hpp"
#include <algorithm>            // TODO: Move to .cpp

/**
 * Set frequency based on the currently executing process.
 *
 * The requested frequencies for processes are validated at runtime, and when a collision
 * occurs (multiple processes request different frequencies for the same CPU cluster), an
 * error message is shown.
 */
class PowerPolicy_PerProcess : public PmPowerPolicy
{
private:
    struct CpuCluster {
        CpufreqPolicy& cf_policy;
        std::vector<Process *> requesting_procs {};
        CpuCluster(CpufreqPolicy& cp) : cf_policy(cp) {}
    };
    std::vector<CpuCluster> clusters {};

public:
    PowerPolicy_PerProcess()
    {
        for (auto &p : pm.policy_iter()) {
            if (!p.available_frequencies)
                throw runtime_error("Cannot list available frequencies for the CPU");
            clusters.emplace_back(p);
        }
    }

    // Validate that the requested frequencies are available
    void validate(const Windows &windows) override
    {
        for (const Window &win : windows) {
            for (const Slice &slice : win.slices) {
                for (const Partition *part : { slice.sc, slice.be }) {
                    if (!part) continue;
                    for (const Process &proc : part->processes) {
                        if (!proc.requested_frequency) continue;
                        for (const CpuCluster &cluster : clusters) {
                            if (!(slice.cpus & cluster.cf_policy.affected_cores)) continue;
                            cluster.cf_policy.validate_frequency(proc.requested_frequency.value());
                        }
                    }
                }
            }
        }
    }
    bool supports_per_process_frequencies() override { return true; }


    void on_process_start(Process &proc) override
    {
        for (CpuCluster &cc : clusters) {
            if (proc.requested_frequency &&
                proc.part.current_cpus() & cc.cf_policy.affected_cores) {
                // process is requesting a fixed frequency, check if it is
                //  compatible with existing locks; all locking processes must have requested the
                //  same frequency, so it suffices to check the first one
                if (!cc.requesting_procs.empty() &&
                    cc.requesting_procs[0]->requested_frequency != proc.requested_frequency) {
                    throw std::runtime_error(
                      fmt::format("Scheduled processes require different frequencies on the CPU(s) "
                                  "{}: '{}', '{}' (process 1: '{}', process 2: '{}')",
                                  cc.cf_policy.affected_cores.as_list(),
                                  cc.requesting_procs[0]->requested_frequency.value(),
                                  proc.requested_frequency.value(),
                                  cc.requesting_procs[0]->argv,
                                  proc.argv));
                }
                cc.requesting_procs.push_back(&proc);
                cc.cf_policy.write_frequency(proc.requested_frequency.value());
            }
        }
    }

    void on_process_end(Process &process) override
    {
	for (auto &cc : clusters) {
	    auto deleted = std::remove(begin(cc.requesting_procs), end(cc.requesting_procs), &process);
	    cc.requesting_procs.erase(deleted, end(cc.requesting_procs));
	}
    }
};

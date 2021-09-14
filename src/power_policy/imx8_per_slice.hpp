#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"
#include "slice.hpp"
#include <optional>

/**
 * Set frequency based on the currently executing slice.
 *
 * The requested frequencies for slices are validated at runtime, and when a collision
 * occurs (multiple slicees request different frequencies for the same CPU cluster), an
 * error message is shown.
 */
class PowerPolicy_Imx8_PerSlice : public PmPowerPolicy
{
private:
    // currently, this is hardcoded for i.MX8, but should be quite simply extensible
    //  for other CPU cluster layouts
    std::array<CpufreqPolicy *, 2> policies{ &pm.get_policy("policy0"), &pm.get_policy("policy4") };

public:
    PowerPolicy_Imx8_PerSlice()
    {
        for (auto &p : pm.policy_iter())
            if (!p.available_frequencies)
                throw runtime_error("Cannot list available frequencies for the CPU"
                                    " - are you sure you're running DEmOS on an i.MX8?");
    }

    // Validate that the requested frequencies are available
    void validate(const Windows &windows) override
    {
        for (const Window &win : windows) {
            for (const Slice &slice : win.slices) {
                if (!slice.requested_frequency) continue;
                for (const CpufreqPolicy *policy : policies) {
                    if (!(slice.cpus & policy->affected_cores)) continue;
                    policy->validate_frequency(slice.requested_frequency.value());
                }
            }
        }
    }
    bool supports_per_slice_frequencies() override { return true; }


    void on_window_start(Window &win) override
    {
        for (CpufreqPolicy *cp : policies) {
            std::optional<CpuFrequencyHz> freq = std::nullopt;
            for (const Slice &slice : win.slices) {
                if (slice.requested_frequency && slice.cpus & cp->affected_cores) {
                    // slice is requesting a fixed frequency, check that all slices on the
                    // same CPU cluster request the same frequency.
                    if (freq.has_value() && freq.value() != slice.requested_frequency) {
                        throw std::runtime_error(fmt::format(
                          "Scheduled slices require different frequencies on the CPU(s) "
                          "{}: '{}', '{}'",
                          cp->affected_cores.as_list(),
                          freq.value(),
                          slice.requested_frequency.value()));
                    }
                    freq = slice.requested_frequency;
                }
            }
            if (freq.has_value()) cp->write_frequency(freq.value());
        }
    }
};

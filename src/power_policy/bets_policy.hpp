#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"
#include "lib/file_lib.hpp"
#include <fstream>
#include <iostream>
#include "slice.hpp"
#include <optional>

/**
 * Based on imx8_per_slice policy, intended for Best Effort Task Scheduler (BETS).
 */
class Bets_policy : public PmPowerPolicy
{
protected:
    // currently, this is hardcoded for i.MX8, but should be quite simply extensible
    //  for other CPU cluster layouts
    std::array<CpufreqPolicy *, 2> policies{ &pm.get_policy("policy0"), &pm.get_policy("policy4") };
    float upper_threshold;
    float lower_threshold;      //TODO: another argument or is fixed margin of 2°C good enough?
    bool reactive_policy_in_place = false;
    const string temp_path_c0 = "/sys/devices/virtual/thermal/thermal_zone0/temp";

public:
    Bets_policy(const std::string &temperature_threshold)
    {
        upper_threshold = std::stof(temperature_threshold);
        lower_threshold = upper_threshold - 2;
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

    float get_temperature_on_c0(){
        auto is = file_open<std::ifstream>(temp_path_c0);
        float temp;
        is >> temp;
        return temp / 1000.0;
    }

    virtual void execute_policy(CpufreqPolicy *cp, CpuFrequencyHz &freq, Window &win){
        cp->write_frequency(freq);
    }

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
            if (freq.has_value()) {
                if ((!reactive_policy_in_place && get_temperature_on_c0() <= upper_threshold) ||
                        (reactive_policy_in_place && get_temperature_on_c0() <= lower_threshold)) {
                    reactive_policy_in_place = false;
                    cp->write_frequency(freq.value());
                } else {
                    reactive_policy_in_place = true;
                    execute_policy(cp, freq.value(), win);
                }
            }
        }
    }
};

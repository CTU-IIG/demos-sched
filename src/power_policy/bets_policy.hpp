#pragma once

#include "_power_policy.hpp"
#include "power_manager.hpp"
#include "lib/file_lib.hpp"
#include <fstream>
#include <iostream>
#include "slice.hpp"
#include <optional>
#include <ctime>
#include <numeric>
#include <vector>


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
    float lower_threshold;
    time_t timer;
    const time_t limit = 10;
    bool temperature_inertia_period = false;
    int level = 0;
    const string temp_path_c0 = "/sys/devices/virtual/thermal/thermal_zone0/temp";
    std::vector<float> temperatures;
    int oldest = 0;

public:
    Bets_policy(const std::string &temperature_threshold)
    {
        upper_threshold = std::stof(temperature_threshold);
        lower_threshold = upper_threshold - 1;
        temperatures = std::vector<float>(10);
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

    float get_temp(){
        this->temperatures[oldest] = this->get_temperature_on_c0();
        oldest = (oldest + 1) % temperatures.size();
        float sum = std::accumulate(this->temperatures.begin(), this->temperatures.end(), 0);
        return sum / this->temperatures.size();
    }

    virtual void execute_policy(CpufreqPolicy *cp, CpuFrequencyHz &freq, Window &win){
        cp->write_frequency(freq);
    }

    void change_level(){
        float avg_temp = this->get_temp();
        if (this->temperature_inertia_period){
            if (time(0) - this->timer >= this->limit){
                this->temperature_inertia_period = false;
            }
            return;       //Change is already in progress
        }

        if (avg_temp > upper_threshold){
            this->temperature_inertia_period = true;
            this->timer = time(0);
            this->level++;
            std::cout << "level=" << this->level << std::endl;
            return;
        }

        if (avg_temp < lower_threshold && level > 0){
            this->temperature_inertia_period = true;
            this->timer = time(0);
            this->level--;
            std::cout << "level=" << this->level << std::endl;
            return;
        }
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
                change_level();
                if (this->level == 0){
                    cp->write_frequency(freq.value());
                } else {
                    execute_policy(cp, freq.value(), win);
                }
            }
        }
    }
};

#pragma once

#include "bets_policy.hpp"

/**
 * In case of temperature threshold overstepping lower frequencies by one step (if possible).
 *
 * Based on imx8_per_slice policy.
 */
class Bets_soft_throttle : public Bets_policy
{
public:
    Bets_soft_throttle(const std::string &temperature_threshold) : Bets_policy(temperature_threshold) {}

    void execute_policy(CpufreqPolicy *cp, CpuFrequencyHz &freq, Window &win) override {
	auto freqs = cp->available_frequencies.value();
        auto it = std::find(freqs.begin(), freqs.end(), freq);
        if (it != freqs.end()){
            it = it == freqs.begin() ? it : --it;   // If it can be lowered, lower it.
            cp->write_frequency(*it);
        } else {
            throw std::runtime_error(fmt::format(
                    "Frequency not found in available frequencies "
                    "'{}'",
                    freq));
        }
    }
};

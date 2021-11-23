#pragma once

#include "bets_policy.hpp"

/**
 * In case of temperature threshold overstepping set frequencies to the lowest possible frequency.
 *
 * Based on imx8_per_slice policy.
 */
class Bets_hard_throttle : public Bets_policy
{
public:
    Bets_hard_throttle(const std::string &temperature_threshold) : Bets_policy(temperature_threshold) {}

    void execute_policy(CpufreqPolicy *cp, CpuFrequencyHz &freq, Window &win) override {
        cp->write_frequency(cp->min_frequency);
    }
};

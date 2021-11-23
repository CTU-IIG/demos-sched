#pragma once

#include "bets_policy.hpp"
#include <map>
#include <limits>
#include <iostream>

/**
 * In case of temperature threshold overstepping skip a task in a fair fashion.
 *
 * Based on imx8_per_slice policy.
 */
class Bets_skip : public Bets_policy
{
private:
    std::map<std::string, int> skip_counter;
public:
    Bets_skip(const std::string &temperature_threshold) : Bets_policy(temperature_threshold) {}

    void execute_policy(CpufreqPolicy *cp, CpuFrequencyHz &freq, Window &win) override {
        cp->write_frequency(freq);

        int min = std::numeric_limits<int>::max();
        std::string candidate;
        int can_idx = 0;
        int i = 0;
        for (auto &s : win.slices){
            std::string key = s.be->get_name();
            if (key.find("sleep") == std::string::npos){ //Sleep process is used for idle time.
                if(!skip_counter.count(key)){
                    skip_counter.insert(std::pair<std::string, int>(key, 0));
                    min = 0;
                    candidate = key;
                    can_idx = i;
                } else {
                    if (skip_counter[key] < min){
                        min = skip_counter[key];
                        can_idx = i;
                        candidate = key;
                    }
                }
            }
            i++;
        }
        skip_counter[candidate]++;

        for (int j = 0; j < win.slices.size(); j++){
            win.to_be_skipped[j] = false;
        }
        win.to_be_skipped[can_idx] = true;
    }
};


#pragma once

#include "bets_policy.hpp"
#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>


bool cmp(std::pair<std::string, int>& a, std::pair<std::string, int>& b) {
    return a.second < b.second;
}

/**
 * In case of temperature threshold overstepping skip a task in a fair fashion.
 *
 * Based on imx8_per_slice policy.
 */
class Bets_skip : public Bets_policy
{
private:
    std::vector<std::pair<std::string, int>> candidates;
    std::vector<std::string> present;
public:
    Bets_skip(const std::string &temperature_threshold) : Bets_policy(temperature_threshold) {}


    void sort_candidates(){
        std::sort(candidates.begin(), candidates.end(), cmp);
    }

    void safe_pushback(std::string key){
        for (auto &p : candidates){
            if (p.first == key){
                return;
            }
        }
        candidates.push_back(std::pair<std::string, int>(key, 0));
    }

    void load(Window &win){
        present = std::vector<std::string>();
        for (auto &s : win.slices){
            std::string key = s.be->get_name();
            if (key.find("sleep") == std::string::npos){ //Sleep process is used for idle time. We are not skipping it.
                present.push_back(key);
                 safe_pushback(key);
            }
        }
    }

    int is_present(std::string key){
        for (int i = 0; i < present.size(); i++){
            if (present[i] == key){
                return i;
            }
        }
        return -1;
    }

    void execute_policy(CpufreqPolicy *cp, CpuFrequencyHz &freq, Window &win) override {
        cp->write_frequency(freq);
        if (cp->name != "policy0"){  //Skip policy works over all clusters, it makes settings only on the first one.
            return;
        }

        if(this->level > 5){
            this->level = 5;
        }
        int skip_counter = 0;
        for (int j = 0; j < win.slices.size(); j++){
            win.to_be_skipped[j] = false;
        }
        load(win);
        sort_candidates();
        // Iterate trough candidates, until up to the level amounth of them is skipped, but at least 1 is kept.
        for(int i = 0; i < candidates.size() && skip_counter < this->level && present.size() - skip_counter > 1; i++){
            int idx = is_present(candidates[i].first);
            if (idx != -1){
                win.to_be_skipped[idx] = true;
                candidates[i].second += win.length.count();
                skip_counter++;
            }
        }
    }
};


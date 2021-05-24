#include "power_policy/_power_policy.hpp"
#include "log.hpp"
#include <algorithm>

#include "power_policy/none.hpp"
#include "power_policy/fixed_low.hpp"
#include "power_policy/fixed_high.hpp"
#include "power_policy/min_be.hpp"

std::unique_ptr<PowerPolicy> PowerPolicy::setup_power_policy(std::string policy_name)
{
    // convert to lowercase
    std::transform(policy_name.begin(), policy_name.end(), policy_name.begin(), ::tolower);

    if (policy_name.empty() || policy_name == "none") {
        logger->info(
          "Power management disabled (to enable, select a power policy using the -p parameter)");
        return std::make_unique<PowerPolicy_None>();
    }
    if (policy_name == "minbe") {
        return std::make_unique<PowerPolicy_MinBE>();
    }
    if (policy_name == "low") {
        return std::make_unique<PowerPolicy_FixedLow>();
    }
    if (policy_name == "high") {
        return std::make_unique<PowerPolicy_FixedHigh>();
    }
    throw std::runtime_error("Unknown power policy selected: " + policy_name);
}

#include "power_policy/_power_policy.hpp"
#include "log.hpp"
#include <algorithm>
#include <map>
#include <string>

// TODO: MK: possibly create a meson build step that lists power policies
//  inside this directory, and generates both includes and PP(...) calls
#include "power_policy/high.hpp"
#include "power_policy/imx8_alternating.hpp"
#include "power_policy/imx8_fixed.hpp"
#include "power_policy/low.hpp"
#include "power_policy/minbe.hpp"
#include "power_policy/none.hpp"


// after multiple template-based attempts, a macro solution was chosen,
//  as it felt the most readable and extensible

#define ARR_ARGS_0(arr_name)
#define ARR_ARGS_1(arr_name) arr_name[0]
#define ARR_ARGS_2(arr_name) ARR_ARGS_1(arr_name), arr_name[1]
#define ARR_ARGS_3(arr_name) ARR_ARGS_2(arr_name), arr_name[2]
#define ARR_ARGS_4(arr_name) ARR_ARGS_3(arr_name), arr_name[3]
#define PP(name, class_, arg_count)                                                                     \
    { /* this is std::pair */                                                                           \
        name, [](const std::vector<std::string> &policy_args) {                                         \
            if (arg_count != policy_args.size()) { /* NOLINT(readability-container-size-empty) */       \
                throw runtime_error(                                                                    \
                    "Incorrect number of parameters for the '" name "' power policy; got " +            \
                    std::to_string(policy_args.size()) + ", expected " #arg_count);                     \
            }                                                                                           \
            return std::make_unique<class_>(ARR_ARGS_##arg_count(policy_args));                         \
        }                                                                                               \
    }

static const std::map<std::string,
                      std::function<std::unique_ptr<PowerPolicy>(const std::vector<std::string> &)>>
  power_policies{
      // the function in each of these macros validates the number of
      //  arguments and creates a unique_ptr<PowerPolicy_...>, passing the arguments
      //  provided by the user
      PP("minbe", PowerPolicy_MinBE, 0),
      PP("low", PowerPolicy_FixedLow, 0),
      PP("high", PowerPolicy_FixedHigh, 0),
      PP("imx8_alternating", PowerPolicy_Imx8_Alternating, 4),
      PP("imx8_fixed", PowerPolicy_Imx8_Fixed, 2),
  };


static std::unique_ptr<PowerPolicy> instantiate_power_policy(
  const std::string &policy_name,
  const std::vector<std::string> &policy_args)
{
    if (policy_name.empty()) {
        logger->info(
          "Power management disabled (to enable, select a power policy using the -p parameter)");
        return std::make_unique<PowerPolicy_None>();
    }
    if (policy_name == "none") {
        logger->info("Power management disabled ('none' policy selected)");
        return std::make_unique<PowerPolicy_None>();
    }

    logger->info("Activating power management (power policy: '{}', args: '{}')",
                 policy_name,
                 fmt::join(policy_args, ","));

    try {
        return power_policies.at(policy_name)(policy_args);
    } catch (const std::out_of_range &) {
        std::string all_pp;
        for (auto const& [pp, val] : power_policies)
            all_pp += (all_pp.empty() ? "" : ", ") + pp;
        throw std::runtime_error("Unknown power policy selected: " + policy_name + "; available policies: " + all_pp);
    }
}

std::unique_ptr<PowerPolicy> PowerPolicy::setup_power_policy(const std::string &policy_str)
{
    // parse power policy string
    // format: <power_policy_name>:<arg1>,<arg2>,...
    size_t pp_name_length = policy_str.find(':');
    std::string policy_name{};
    std::vector<std::string> args_vec{};

    if (pp_name_length == std::string::npos) {
        // no separator, only policy name
        policy_name = policy_str;
    } else {
        policy_name = policy_str.substr(0, pp_name_length);
        std::string arg_str = policy_str.substr(pp_name_length + 1);

        // parse arg_str
        size_t separator_pos;
        while ((separator_pos = arg_str.find(',')) != std::string::npos) {
            args_vec.push_back(arg_str.substr(0, separator_pos));
            arg_str.erase(0, separator_pos + 1);
        }
        args_vec.push_back(arg_str);
    }

    // convert policy name to lowercase
    std::transform(policy_name.begin(), policy_name.end(), policy_name.begin(), ::tolower);

    return instantiate_power_policy(policy_name, args_vec);
}

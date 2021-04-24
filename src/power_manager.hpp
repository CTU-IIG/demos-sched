#pragma once

#include "cpufreq_policy.hpp"
#include "lib/file_lib.hpp"
#include "log.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <string>

namespace fs = std::filesystem;
using std::runtime_error;
using std::string;

/**
 * Manages CPU frequency scaling using Linux `cpufreq` system.
 *
 * ## Documentation of `cpufreq`:
 * https://www.kernel.org/doc/html/latest/admin-guide/pm/cpufreq.html
 * https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-devices-system-cpu
 *
 *
 * ## CPU frequencies
 * Most CPUs have multiple supported pairs of frequencies and voltages.
 * Switching between these states allows the system to balance higher performance with lower
 * power consumption and thermal output. CPUs usually don't allow setting frequency to any value,
 * but only to discrete predefined values.
 *
 * For example, the ARM Cortex-A53 cluster in an i.MX 8 CPU supports 4 frequencies:
 * 0.6 GHz, 0.896 GHz, 1.104 GHz and 1.2GHz, with voltage varying accordingly.
 *
 *
 * ## CPU core groups (cpufreq policies)
 * On some systems, the CPU frequencies may not be controlled separately per-core.
 * Instead, groups of cores are always running with the same configuration.
 * Linux represents this with the so called "cpufreq policies".
 *
 * For example, the i.MX 8 CPU has 2 clusters of ARM cores - 2 Cortex-A72 and 4 Cortex-A53 cores,
 * with cores of the same type always running at the same frequency (so there are 2 policies).
 *
 *
 * ## Governors
 * There are multiple CPU governors in the Linux kernel. We will be using the "userspace"
 * governor that lets us control the CPU speed manually from userspace.
 * After governor is configured, we can control the CPU core frequency by writing to
 * `/sys/devices/system/cpu/cpufreq/policy<n>/scaling_setspeed`. The closest supported
 * frequency is selected.
 *
 *
 * ## Structure of /sys/devices/system/cpu directory
 * NOTE: to be able to write here, we must be running as root
 * - ./cpufreq/policy<n> represent CPU policies (cpu groups, see above)
 * - ./cpu<n>/cpufreq = symlink to some policy in ./cpufreq/
 * - ./cpu<n>/online = boolean, write 0 to turn the CPU core off
 * ### Structure of ./cpufreq/policy<n> subdirectory
 * - ./scaling_available_frequencies = space-separated list of all supported frequencies
 * - ./scaling_governor = current active governor
 * - ./scaling_setspeed = when scaling_governor is "userspace", writing here changes CPU frequency
 *
 *
 * ## intel_pstate
 * Intel CPUs have custom driver, which has an "active" and "passive" mode. In active mode,
 * the driver manages core p-states internally and only allows setting the max and min frequency.
 *
 * In passive mode, it operates similarly to the generic driver for other platforms - a minor
 * difference is that it does not expose all supported frequencies in the
 * `./scaling_available_frequencies` file, as the allowed p-states are too granular to list them
 * individually.
 *
 * TODO: sleep states, and what about them?
 *
 * TODO: would it be useful to create read-only CpufreqPolicy objects for monitoring
 *  even when we don't have write access to cpufreq dir?
 *
 * TODO: in case DEmOS is killed or crashes without stack unwinding, the CPU governors will not
 *  be reset to original values; fundamentally, the only "reliable" solutions are:
 *   1) let the user manually change the governors before running DEmOS and then change it back
 *      after he's done, but that's not really reliable, it just lets us say "hey, it's your fault
 *      for not setting it back manually and losing your work to a thermal shutdown"
 *   2) run DEmOS either under systemd, or use some other launcher that ensures setup and teardown
 *      are correctly executed on each invocation, even if DEmOS itself crashes
 *   Technically, 1) is supported now, as DEmOS will not touch governors if they are already all
 *    set to "userspace". Still, 2) would imo be a better solution.
 *
 *
 * ## Usage examples
 *
 * ### Set all cores to max frequency (this works even when cpufreq is not supported)
 * When cpufreq is not accessible, the iterator will be empty and this will be a no-op.
 * ```
 * for (auto &p : power_manager.policy_iter()) {
 *     p.write_frequency(p.max_frequency);
 * }
 * ```
 *
 * ### Set min frequency for A53 cluster on i.MX8
 * It's a good idea to first check if we can use the power manager (cpufreq is supported
 * and accessible), otherwise the policy lookup will fail (`power_manager.is_active()`)
 * ```
 * if (power_manager.is_active()) {
 *     auto &p = power_manager.get_policy("policy0");
 *     p.write_frequency(p.min_frequency);
 * }
 * ```
 *
 * ### Set frequency for the 3rd core to 1.20 GHz (and for all other cores under the same policy)
 * NOTE: here, you have to know the supported frequency beforehand
 * ```
 * if (power_manager.is_active()) {
 *     power_manager.set_core_frequency(3, 1200 * 1000000);
 * }
 * ```
 */
class PowerManager
{
private: ///////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Indicates if all required components are supported and we are able to control CPU
     * frequency scaling manually. If false, all methods on this instance are no-ops.
     */
    bool active = true;
    string original_intel_pstate_status{};
    // use list over vector, because vector would require implementing move constructor
    //  for CpufreqPolicy, and for "random" access, there's already the `policy_by_name` map
    std::list<CpufreqPolicy> policies{};
    /** Maps from core index to associated cpufreq policy. */
    std::map<uint32_t, std::reference_wrapper<CpufreqPolicy>> associated_policies{};
    /** Maps from policy name to matching cpufreq policy. */
    std::map<string, std::reference_wrapper<CpufreqPolicy>> policy_by_name{};

    using policy_iterator = std::list<CpufreqPolicy>::iterator;

public: ////////////////////////////////////////////////////////////////////////////////////////////
    // delete copy and move constructors
    PowerManager(const PowerManager &) = delete;
    PowerManager &operator=(const PowerManager &) = delete;
    PowerManager(PowerManager &&) = delete;
    PowerManager &operator=(PowerManager &&) = delete;

    PowerManager()
    {
        logger->debug("Checking for `cpufreq` support and permissions");
        if (!check_support()) {
            active = false;
            return;
        }

        // now we know cpufreq is supported and we have write access

        // first, we must figure out if we're running on Intel CPU, as it uses the custom
        // `intel_pstate` driver, and we have to switch it to passive mode if we are to set
        // the frequencies manually
        logger->trace("Checking for `intel_pstate` driver");
        if (system_has_intel_cpu()) {
            logger->debug("Detected `intel_pstate` driver");
            if (!setup_intel_pstate_driver()) {
                active = false;
                return;
            }
        } else {
            logger->trace("`intel_pstate` driver is not used");
        }

        logger->debug("Setting up `cpufreq` policy wrappers and configuring governors...");
        setup_policy_objects();

        logger->info("Power management active");
    }

    ~PowerManager()
    {
        if (!active) return;

        if (!original_intel_pstate_status.empty()) {
            reset_intel_pstate_driver();
        }
        // this is done automatically in CpufreqPolicy destructor
        logger->debug("Resetting cpufreq governors to original values");
    }

    /**
     * Returns true if we have control over CPU frequency scaling.
     * If false, you should not call any methods on this instance.
     */
    [[nodiscard]] bool is_active() const { return active; }

    /**
     * Lookup policy by name.
     * @param name - name of the directory in /sys/devices/system/cpu/cpufreq
     *  representing given policy; e.g. "policy0", "policy4",...
     */
    CpufreqPolicy &get_policy(const string &name) { return policy_by_name.at(name); }
    /** Returns policy object that controls the `core_i`-th CPU core. */
    CpufreqPolicy &get_core_policy(CpuIndex core_i) { return associated_policies.at(core_i); }

    /** Sets core frequency of the cpufreq policy controlling `core_i`-th CPU core. */
    void set_core_frequency(CpuIndex core_i, CpuFrequencyHz freq)
    {
        // TODO: is this a good API?
        //  I think it might be better to remove it and keep
        //  only `get_policy(name)` and `policy_iter()`
        if (!active) return;
        get_core_policy(core_i).write_frequency(freq);
    }

    class PolicyIterator
    {
    public:
        explicit PolicyIterator(PowerManager *p)
            : pm(p)
        {}
        policy_iterator begin() { return pm->policies.begin(); }
        policy_iterator end() { return pm->policies.end(); }

    private:
        PowerManager *pm;
    };

    [[nodiscard]] size_t policy_count() const { return policies.size(); }
    PolicyIterator policy_iter() { return PolicyIterator(this); }

private: ///////////////////////////////////////////////////////////////////////////////////////////
    static bool check_support()
    {
        // just checking .../cpufreq is not enough to detect usable cpufreq support; for example,
        // the Ubuntu environment in GitHub Actions has cpufreq dir, but it's empty;
        // using policy0 seems more reliable

        // FIXME: is it guaranteed that there will be a `policy0`?
        fs::path path("/sys/devices/system/cpu/cpufreq/policy0/scaling_governor");
        logger->trace("Using following path to test `cpufreq` support: `{}`", path.string());
        try {
            file_open<std::ofstream>(path);
            return true;
        } catch (IOError &err) {
            // does not exist
            if (err.errno_() == ENOENT) {
                logger->warn(
                  "`cpufreq` seems not to be supported by your kernel."
                  "\n\tDEmOS will run without control over CPU frequency scaling, which may result"
                  "\n\tin impaired predictability and thermal properties of the running "
                  "configuration.");
                return false;
            }
            // exists, but we cannot write into it
            if (err.errno_() == EACCES) {
                logger->warn(
                  "DEmOS doesn't have permission to control CPU frequency scaling."
                  "\n\tDEmOS will run without frequency scaling support, which may result in"
                  "\n\timpaired predictability and thermal properties of the running configuration."
                  "\n\t"
                  "\n\tTo resolve, either run DEmOS as root, or use a more granular mechanism to"
                  "\n\tprovide read-write access to files under the `/sys/devices/system/cpu/` "
                  "directory.");
                return false;
            }

            // some other error
            std::throw_with_nested(
              runtime_error("Failed to open CPU policy governor file for writing"));
        }
    }

    static bool system_has_intel_cpu()
    {
        return fs::exists("/sys/devices/system/cpu/intel_pstate");
    }

    bool setup_intel_pstate_driver()
    {
        auto fs_status = file_open<std::fstream>("/sys/devices/system/cpu/intel_pstate/status");

        fs_status >> original_intel_pstate_status;

        if (original_intel_pstate_status == "off") {
            // um, what?
            // this probably means that someone already overridden their intel_pstate driver
            //  and is using some other driver
            // I don't wanna deal with this so lets just deactivate power management for demos
            logger->warn("It seems your system has an Intel CPU, but a non-standard driver is used "
                         "for CPU frequency scaling. DEmOS will run without custom CPU frequency "
                         "scaling support. Switch to the `intel_pstate` driver to enable it.");
            return false;
        }

        if (original_intel_pstate_status == "active") {
            // we have to switch to passive mode, otherwise we have
            //  no control over frequency scaling
            logger->debug("Switching to `passive` mode for the `intel_pstate` driver");
            fs_status << "passive";
        }
        return true;
    }

    void reset_intel_pstate_driver()
    {
        if (original_intel_pstate_status == "passive") return;

        logger->debug("Resetting `intel_pstate` driver mode to `{}`", original_intel_pstate_status);
        auto os = file_open<std::ofstream>("/sys/devices/system/cpu/intel_pstate/status");
        os << original_intel_pstate_status;
    }

    void setup_policy_objects()
    {
        fs::directory_iterator iter("/sys/devices/system/cpu/cpufreq");
        for (auto &policy_entry : iter) {
            fs::path path = policy_entry.path();
            if (0 != path.filename().string().rfind("policy", 0)) {
                // not cpufreq/policy* dir (sometimes, governors also have configuration dirs here)
                continue;
            }

            policies.emplace_back(path);
            CpufreqPolicy &policy = policies.back();

            policy_by_name.insert({ policy.name, std::ref(policy) });

            for (CpuIndex cpu_i : policy.affected_cores) {
                associated_policies.insert({ cpu_i, std::ref(policy) });
            }
        }
    }
};

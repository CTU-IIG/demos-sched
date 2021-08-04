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
 * power consumption and thermal output. CPUs usually don't allow setting frequency to any desired value,
 * but only to a set of discrete predefined values.
 *
 * For example, the ARM Cortex-A53 cluster in an i.MX 8 CPU supports 4 frequencies:
 * 0.6 GHz, 0.896 GHz, 1.104 GHz and 1.2GHz, with voltage varying accordingly.
 *
 *
 * ## CPU core groups (`cpufreq` policies)
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
 * After the governor is configured, we can control the CPU core frequency by writing to
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
 * ## `intel_pstate`
 * https://www.kernel.org/doc/html/v4.14/admin-guide/pm/intel_pstate.html
 * Intel CPUs have custom driver, which has an "active" and "passive" mode. In active mode,
 * the driver manages core p-states internally and only allows setting the max and min frequency.
 *
 * In passive mode, it operates similarly to the generic driver for other platforms - a minor
 * difference is that it does not expose all supported frequencies in the
 * `./scaling_available_frequencies` file, as the allowed p-states are too granular to list them
 * individually.
 *
 * TODO: sleep states, and what about them?
 *  managing sleep states would require a kernel module
 *  https://www.kernel.org/doc/html/latest/admin-guide/pm/cpuidle.html
 *  https://www.kernel.org/doc/html/latest/admin-guide/pm/intel_idle.html
 *
 * TODO: in case DEmOS is killed or crashes without stack unwinding, CPU governors will not
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
 * ### Set all cores to max frequency
 * ```
 * for (auto &p : power_manager.policy_iter()) {
 *     p.write_frequency(p.max_frequency);
 * }
 * ```
 *
 * ### Set min frequency for the A53 cluster on i.MX8
 * ```
 * auto &p = power_manager.get_policy("policy0");
 * p.write_frequency(p.min_frequency);
 * ```
 *
 * ### Set the A72 cluster on i.MX8 to fixed frequency
 * ```
 * auto &p = power_manager.get_policy("policy4");
 * p.write_frequency(1296 * 1000000);
 * ```
 */
class PowerManager
{
private: ///////////////////////////////////////////////////////////////////////////////////////////
    string original_intel_pstate_status{};
    // use list over vector, because vector would require implementing move constructor
    //  for CpufreqPolicy, and for "random" access, there's already the `policy_by_name` map
    std::list<CpufreqPolicy> policies{};
    /** Maps from policy name to matching `cpufreq` policy. */
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
        check_support();

        // now we know cpufreq is supported and we have write access

        // first, we must figure out if we're running on Intel CPU, as it uses the custom
        // `intel_pstate` driver, and we have to switch it to passive mode if we are to set
        // the frequencies manually
        logger->trace("Checking for `intel_pstate` driver");
        if (system_has_intel_cpu()) {
            logger->debug("Detected `intel_pstate` driver");
            setup_intel_pstate_driver();
        } else {
            logger->trace("`intel_pstate` driver is not used");
        }

        logger->debug("Setting up `cpufreq` policy wrappers and configuring governors...");
        setup_policy_objects();

        logger->debug("Power manager set up");
    }

    ~PowerManager()
    {
        // FIXME: we should first restore governors and THEN reset intel_pstate driver;
        //  also investigate if the way it currently is breaks anything
        if (!original_intel_pstate_status.empty()) {
            reset_intel_pstate_driver();
        }
        // this is done automatically in CpufreqPolicy destructor
        logger->debug("Resetting `cpufreq` governors to original values");
    }

    /**
     * Lookup policy by name.
     * @param name - name of the directory in /sys/devices/system/cpu/cpufreq
     *  representing given policy; e.g. "policy0", "policy4",...
     */
    CpufreqPolicy &get_policy(const string &name) { return policy_by_name.at(name); }

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

    PolicyIterator policy_iter() { return PolicyIterator(this); }

private: ///////////////////////////////////////////////////////////////////////////////////////////
    static void check_support()
    {
        // just checking .../cpufreq is not enough to detect usable cpufreq support; for example,
        // the Ubuntu environment in GitHub Actions has cpufreq dir, but it's empty;
        // using policy0 seems more reliable

        // FIXME: is it guaranteed that there will be a `policy0`?
        fs::path path("/sys/devices/system/cpu/cpufreq/policy0/scaling_governor");
        logger->trace("Using following path to test cpufreq support: '{}'", path.string());
        try {
            file_open<std::ofstream>(path);
        } catch (IOError &err) {
            // does not exist
            if (err.errno_() == ENOENT) {
                throw runtime_error("Cannot activate power management policy, as"
                                    " `cpufreq` seems not to be supported by your kernel."
                                    " Re-run DEmOS without explicitly setting power policy to"
                                    " run without active power management.");
            }
            // exists, but we cannot write into it
            if (err.errno_() == EACCES) {
                throw runtime_error(
                  "Cannot activate power management policy, as DEmOS doesn't have permission"
                  " to control CPU frequency scaling."
                  " To provide access, either run DEmOS as root, or use a more granular mechanism"
                  " to provide read-write access to files under the `/sys/devices/system/cpu/`"
                  " directory. Alternatively, re-run DEmOS without explicitly setting power policy"
                  " to run without active power management.");
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

    void setup_intel_pstate_driver()
    {
        auto fs_status = file_open<std::fstream>("/sys/devices/system/cpu/intel_pstate/status");

        fs_status >> original_intel_pstate_status;

        if (original_intel_pstate_status == "off") {
            // um, what?
            // this probably means that someone already overridden their intel_pstate driver
            //  and is using some other driver
            // I don't wanna deal with this so lets just deactivate power management for demos
            throw runtime_error(
              "It seems your system has an Intel CPU, but a non-standard driver is used"
              " for CPU frequency scaling. Switch to the `intel_pstate` driver to enable it."
              " Alternatively, re-run DEmOS without explicitly setting power policy"
              " to run without active power management.");
        }

        if (original_intel_pstate_status == "active") {
            // we have to switch to passive mode, otherwise we have
            //  no control over frequency scaling
            logger->debug("Switching to `passive` mode for the `intel_pstate` driver");
            fs_status << "passive";
        }
    }

    void reset_intel_pstate_driver()
    {
        if (original_intel_pstate_status == "passive") return;

        logger->debug("Resetting `intel_pstate` driver mode to '{}'", original_intel_pstate_status);
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

            CpufreqPolicy &policy = policies.emplace_back(path);
            policy_by_name.insert({ policy.name, std::ref(policy) });
        }
    }
};

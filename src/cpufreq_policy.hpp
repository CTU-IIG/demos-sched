#pragma once

#include "lib/file_lib.hpp"
#include "log.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <lib/assert.hpp>
#include <optional>
#include <set>
#include <string>
#include <utility>

namespace fs = std::filesystem;
using std::runtime_error;
using std::string;

/** CPU core frequency, in Hz. */
using CpuFrequencyHz = uint64_t;

/**
 * NOTE: This class assumes that cpufreq is supported and write-able,
 *       and if `intel_pstate` driver is used, it is switched to passive mode.
 */
class CpufreqPolicy
{
private: ///////////////////////////////////////////////////////////////////////////////////////////
    const fs::path policy_dir;
    string current_governor;
    // use direct `write(...)` calls over fstreams to improve performance
    int fd_freq;

public: ////////////////////////////////////////////////////////////////////////////////////////////
    const string name;
    const string original_governor;
    const CpuFrequencyHz min_frequency;
    const CpuFrequencyHz max_frequency;
    const std::optional<std::set<CpuFrequencyHz>> available_frequencies;
    const cpu_set affected_cores;
    const bool active;

public: ////////////////////////////////////////////////////////////////////////////////////////////
    // delete copy and (implicitly) move constructors
    // otherwise we'd have to deal with move semantics w.r.t. resetting governor state,
    //  which would be too much pointless work
    CpufreqPolicy(const CpufreqPolicy &) = delete;
    CpufreqPolicy &operator=(const CpufreqPolicy &) = delete;

    explicit CpufreqPolicy(fs::path policy_dir_path)
        : policy_dir{ std::move(policy_dir_path) }
        , current_governor{ read_governor() }
        , fd_freq{ -1 } // opened after governor is changed
        , name{ policy_dir.filename() }
        , original_governor{ current_governor }
        , min_frequency{ read_freq_file(policy_dir / "scaling_min_freq") }
        , max_frequency{ read_freq_file(policy_dir / "scaling_max_freq") }
        , available_frequencies{ read_available_frequencies() }
        , affected_cores{ read_affected_cpus() }
        , active{ affected_cores.count() > 0 }
    {
        if (original_governor == "userspace") {
            logger->warn(
              "`cpufreq` governor is already set to 'userspace'."
              "\n\tThis typically means that another program (or another instance of DEmOS)"
              "\n\tis already actively managing CPU frequency scaling from userspace. This DEmOS"
              "\n\tinstance was started with power management enabled, and it will overwrite the"
              "\n\tCPU frequencies the other program may have set up."
              "\n\t"
              "\n\tNote that running multiple DEmOS instances with an active power policy"
              "\n\tis NOT supported, and later instances may crash when the first one exits.");
        }

        write_governor("userspace");

        fs::path freq_path(policy_dir / "scaling_setspeed");
        fd_freq = CHECK(open(freq_path.string().c_str(), O_RDWR | O_NONBLOCK));

        logger->debug("Initialized a cpufreq policy object `{}`; (frequencies: min=`{}`, max=`{}`, "
                      "available: `{}`)",
                      name,
                      freq_to_str(min_frequency),
                      freq_to_str(max_frequency),
                      get_available_freq_str());
    }

    ~CpufreqPolicy()
    {
        close(fd_freq);
        // reset governor to original value
        // if governor was not manually changed, this is a noop
        write_governor(original_governor);
    }

    /** Sets core frequency for all cores under this cpufreq policy. */
    void write_frequency(CpuFrequencyHz freq)
    {
        if (!active) return;

        // check that freq is between min-max and is one of the supported frequencies
        validate_frequency(freq);

        TRACE("Changing CPU frequency to `{}` for `{}`", freq_to_str(freq), name);
        // cpufreq uses kHz, we have Hz
        auto freq_str = std::to_string(freq / 1000);
        CHECK_MSG(write(fd_freq, freq_str.c_str(), freq_str.size()),
                  "Could not set frequency for cpufreq policy `" + name + "`");
    }

private: ///////////////////////////////////////////////////////////////////////////////////////////
    static string freq_to_str(CpuFrequencyHz freq)
    {
        return std::to_string(freq / 1000000) + " MHz";
    }

    string get_available_freq_str()
    {
        if (!available_frequencies) {
            return "unknown";
        }
        string str;
        for (CpuFrequencyHz freq : available_frequencies.value()) {
            str += freq_to_str(freq) + ", ";
        }
        // remove trailing ", "
        str.resize(str.size() - 2);
        return str;
    }

    void validate_frequency(CpuFrequencyHz freq)
    {
        if (freq < min_frequency) {
            throw runtime_error("Attempted to set CPU frequency for `" + name + "` to `" +
                                std::to_string(freq) +
                                "` Hz, which is less than the supported minimum of `" +
                                std::to_string(min_frequency) + "` Hz");
        }
        if (freq > max_frequency) {
            throw runtime_error("Attempted to set CPU frequency for `" + name + "` to `" +
                                std::to_string(freq) +
                                "` Hz, which is more than the supported maximum of `" +
                                std::to_string(max_frequency) + "` Hz");
        }
        if (available_frequencies && available_frequencies->count(freq) == 0) {
            throw runtime_error("Attempted to set CPU frequency for `" + name + "` to `" +
                                std::to_string(freq) +
                                "` Hz, which is not listed as an available frequency for this CPU");
        }
    }

    string read_governor()
    {
        auto is = file_open<std::ifstream>(policy_dir / "scaling_governor", false);
        string current_gov;
        if (!(is >> current_gov)) {
            throw runtime_error("Failed to read current cpufreq governor for policy `" + name +
                                "`");
        }
        return current_gov;
    }

    void write_governor(const string &governor)
    {
        if (!active) return;
        if (governor == current_governor) return;

        logger->trace("Setting cpufreq governor for `{}` to `{}`", name, governor);

        auto os = file_open<std::ofstream>(policy_dir / "scaling_governor", false);
        if (!(os << governor)) {
            throw runtime_error("Failed to set cpufreq governor to `" + governor +
                                "` for policy `" + name + "`");
        }
        current_governor = governor;
    }

    std::optional<std::set<CpuFrequencyHz>> read_available_frequencies()
    {
        std::ifstream is_freq;
        try {
            file_open(is_freq, policy_dir / "scaling_available_frequencies", false);
        } catch (...) {
            logger->trace("Used CPU frequency scaling driver does not support listing "
                          "available CPU scaling frequencies (probably the `intel_pstate` driver)");
            return std::nullopt;
        }

        std::set<CpuFrequencyHz> freqs;
        CpuFrequencyHz freq_khz;
        while (is_freq >> freq_khz) {
            // convert from kHz (used by cpufreq) to Hz
            freqs.insert(freq_khz * 1000);
        }
        ASSERT(is_freq.eof());
        return std::make_optional(freqs);
    }

    cpu_set read_affected_cpus()
    {
        auto is_cpus = file_open<std::ifstream>(policy_dir / "affected_cpus", false);
        cpu_set cpuset;
        uint32_t i;
        while (is_cpus >> i) {
            cpuset.set(i);
        }
        ASSERT(is_cpus.eof());
        return cpuset;
    }

    static CpuFrequencyHz read_freq_file(const fs::path &path)
    {
        auto is = file_open<std::ifstream>(path);
        CpuFrequencyHz freq;
        is >> freq;
        // cpufreq uses kHz, we want Hz
        return freq * 1000;
    }
};

#pragma once

#include "lib/cpu_set.hpp"
#include "lib/file_lib.hpp"
#include "log.hpp"
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <lib/assert.hpp>
#include <lib/check_lib.hpp>
#include <optional>
#include <string>
#include <unistd.h>
#include <utility>

namespace fs = std::filesystem;
using std::runtime_error;
using std::string;

/** CPU core frequency, in Hz. */
// using CpuFrequencyHz = uint64_t;
struct CpuFrequencyHz
{
    uint64_t freq;
    explicit CpuFrequencyHz(uint64_t freq)
        : freq{ freq }
    {}
    bool operator==(const CpuFrequencyHz &rhs) const { return freq == rhs.freq; }
    operator uint64_t() const { return freq; } // NOLINT(google-explicit-constructor)
};

template<>
struct fmt::formatter<CpuFrequencyHz> : fmt::formatter<uint64_t>
{
    template<typename FormatContext>
    auto format(CpuFrequencyHz const &freq, FormatContext &ctx)
    {
        return fmt::format_to(
          ctx.out(), "{}{} MHz", freq.freq % 1000000 == 0 ? "" : "~", freq.freq / 1000000);
    }
};

/**
 * NOTE: This class assumes that `cpufreq` is supported and write-able,
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
    // for small numbers of frequencies (under 12, on my machine), std::vector with linear search
    //  is faster than both std::set and std::unordered_set when checking if a frequency is valid
    // it also supports random access, which is convenient for writing power policies
    const std::optional<std::vector<CpuFrequencyHz>> available_frequencies;
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
        // sometimes, the policy does not affect any cores (e.g. all controlled cores are offline);
        //  attempting to set the frequency would then cause an IO error
        , active{ affected_cores.count() > 0 }
    {
        if (!active) {
            logger->warn("Cpufreq policy '{}' is not active, as all affected cores are offline",
                         name);
        }

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

        logger->debug("Initialized a `cpufreq` policy object '{}'; (affected CPUs: '{}', "
                      "frequencies: min='{}', max='{}', "
                      "available: '{}')",
                      affected_cores.as_list(),
                      name,
                      min_frequency,
                      max_frequency,
                      get_available_freq_str());
    }

    ~CpufreqPolicy()
    {
        close(fd_freq);
        // reset governor to original value
        // if governor was not manually changed, this is a noop
        write_governor(original_governor);
    }

    /**
     * Sets core frequency for all cores under this `cpufreq` policy.
     *
     * NOTE: this call seems to be blocking, and on our i.MX8 board, it apparently takes
     *  on the order of 100µs to complete; the code in window.cpp is structured so that
     *  the PowerPolicy handlers are called in moments where the resulting performance
     *  impact is minimized, but it's still not negligible
     */
    void write_frequency(CpuFrequencyHz freq)
    {
        if (!active) return;

        // check that freq is between min-max and is one of the supported frequencies
        // this check is only called in debug builds, because cpufreq does its own checking
        RUN_DEBUG(validate_frequency(freq));

        TRACE("Changing CPU frequency to '{}' for '{}'", freq, name);
        // cpufreq uses kHz, we have Hz
        auto freq_str = std::to_string(freq / 1000);
        CHECK_MSG(write(fd_freq, freq_str.c_str(), freq_str.size()),
                  "Could not set frequency for `cpufreq` policy '" + name + "'");
    }

    /** Get the n-th lowest frequency available on this `cpufreq` policy. */
    [[nodiscard]] CpuFrequencyHz get_freq(size_t index) const
    {
        if (!available_frequencies) {
            throw runtime_error(
              "Cannot get frequency by index for `cpufreq` policy '" + name +
              "', because the list of available frequencies is not available (some drivers do not "
              "provide it, most notably the `intel_pstate` driver used on Intel processors)");
        }
        if (index >= available_frequencies->size()) {
            throw runtime_error(
              "Tried to get an out-of-bounds frequency #'" + std::to_string(index) +
              "' for `cpufreq` policy '" + name + "' (there are only '" +
              std::to_string(available_frequencies->size()) + "' defined frequencies)");
        }
        return available_frequencies->at(index);
    }

    void write_frequency_i(size_t index)
    {
        if (!active) return;
        write_frequency(get_freq(index));
    }

private: ///////////////////////////////////////////////////////////////////////////////////////////
    string get_available_freq_str()
    {
        if (!available_frequencies) {
            return "unknown";
        }
        return fmt::format("{}", fmt::join(available_frequencies.value(), ", "));
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
        if (available_frequencies &&
            std::find(available_frequencies->begin(), available_frequencies->end(), freq) ==
              available_frequencies->end()) {
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
            throw runtime_error("Failed to read current `cpufreq` governor for policy `" + name +
                                "`");
        }
        return current_gov;
    }

    void write_governor(const string &governor)
    {
        if (!active) return;
        if (governor == current_governor) return;

        logger->trace("Setting `cpufreq` governor for '{}' to '{}'", name, governor);

        auto os = file_open<std::ofstream>(policy_dir / "scaling_governor", false);
        if (!(os << governor)) {
            throw runtime_error("Failed to set `cpufreq` governor to `" + governor +
                                "` for policy `" + name + "`");
        }
        current_governor = governor;
    }

    std::optional<std::vector<CpuFrequencyHz>> read_available_frequencies()
    {
        std::ifstream is_freq;
        try {
            file_open(is_freq, policy_dir / "scaling_available_frequencies", false);
        } catch (...) {
            logger->trace("Used CPU frequency scaling driver does not support listing "
                          "available CPU scaling frequencies (probably the `intel_pstate` driver)");
            return std::nullopt;
        }

        std::vector<CpuFrequencyHz> freqs;
        uint64_t freq_khz;
        while (is_freq >> freq_khz) {
            // convert from kHz (used by cpufreq) to Hz
            freqs.emplace_back(freq_khz * 1000);
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
        uint64_t freq;
        is >> freq;
        // cpufreq uses kHz, we want Hz
        return CpuFrequencyHz{ freq * 1000 };
    }
};

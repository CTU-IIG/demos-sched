#pragma once

#include "file_lib.hpp"
#include "log.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <set>
#include <utility>

namespace fs = std::filesystem;
using std::runtime_error;
using std::string;

/** CPU core frequency, in Hz. */
using CpuFrequencyHz = uint64_t;
/** Index of CPU core. */
using CpuIndex = uint32_t;

/**
 * NOTE: This class assumes that cpufreq is supported and write-able,
 *       and if `intel_pstate` driver is used, it is switched to passive mode.
 */
class CpufreqPolicy
{
private: ///////////////////////////////////////////////////////////////////////////////////////////
    const fs::path policy_dir;
    string current_governor;

public: ////////////////////////////////////////////////////////////////////////////////////////////
    const string name;
    const string original_governor;
    const CpuFrequencyHz min_frequency;
    const CpuFrequencyHz max_frequency;
    const std::optional<std::set<CpuFrequencyHz>> available_frequencies;
    const std::set<CpuIndex> affected_cores;
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
        , name{ policy_dir.filename() }
        , original_governor{ current_governor }
        , min_frequency{ read_freq_file(policy_dir / "scaling_min_freq") }
        , max_frequency{ read_freq_file(policy_dir / "scaling_max_freq") }
        , available_frequencies{ read_available_frequencies() }
        , affected_cores{ read_affected_cpus() }
        , active{ !affected_cores.empty() }
    {
        write_governor("userspace");
        logger->trace("Initialized cpufreq policy object `{}`; (frequencies: min=`{}`, max=`{}`, "
                      "available: `{}`)",
                      name,
                      freq_to_str(min_frequency),
                      freq_to_str(max_frequency),
                      get_available_freq_str());
    }

    ~CpufreqPolicy()
    {
        // reset governor to original value
        // if governor was not manually changed, this is a noop
        write_governor(original_governor);
    }

    /** Retrieves core frequency of cores under this cpufreq policy. */
    CpuFrequencyHz read_frequency()
    {
        return read_freq_file(policy_dir / "scaling_cur_freq");
    }

    /** Sets core frequency for all cores under this cpufreq policy. */
    void write_frequency(CpuFrequencyHz freq)
    {
        if (!active) return;

        // check that freq is between min-max and is one of the supported frequencies
        validate_frequency(freq);

        logger->trace("Changing CPU frequency to `{}` for `{}`", freq_to_str(freq), name);
        try {
            write_freq_file(policy_dir / "scaling_setspeed", freq);
        } catch (...) {
            std::throw_with_nested(
              runtime_error("Could not set frequency for cpufreq policy `" + name + "`"));
        }
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
            logger->debug("Used CPU frequency scaling driver does not support listing "
                          "available CPU scaling frequencies (probably the `intel_pstate` driver)");
            return std::nullopt;
        }

        auto freqs_khz = read_set_from_file_stream<CpuFrequencyHz>(is_freq);
        // convert from kHz (used by cpufreq) to Hz
        std::set<CpuFrequencyHz> freqs_hz;
        for (auto f : freqs_khz) {
            freqs_hz.insert(f * 1000);
        }
        return std::make_optional(freqs_hz);
    }

    std::set<CpuIndex> read_affected_cpus()
    {
        auto is_cpus = file_open<std::ifstream>(policy_dir / "affected_cpus", false);
        return read_set_from_file_stream<CpuIndex>(is_cpus);
    }

    template<typename T>
    static std::set<T> read_set_from_file_stream(std::ifstream &stream)
    {
        // we don't want to set failbait, otherwise the last read at EOF would throw exception
        stream.exceptions(std::ios::badbit);
        std::set<T> items;
        T item;
        while (stream >> item) {
            items.insert(item);
        }
        assert(stream.eof());
        return items;
    }

    static CpuFrequencyHz read_freq_file(const fs::path &path)
    {
        auto is = file_open<std::ifstream>(path);
        CpuFrequencyHz freq;
        is >> freq;
        // cpufreq uses kHz, we want Hz
        return freq * 1000;
    }

    static void write_freq_file(const fs::path &path, CpuFrequencyHz freq)
    {
        auto os = file_open<std::ofstream>(path);
        // cpufreq uses kHz, we have Hz
        os << (freq / 1000);
    }
};

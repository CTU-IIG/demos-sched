#include "config.hpp"
#include "log.hpp"
#include "power_policy/_power_policy.hpp"
#include <cmath>
#include <exception>
#include <lib/assert.hpp>

using namespace std;
using namespace YAML;
namespace fs = std::filesystem;

template<typename T_all>
static cpu_set parse_cpu_set(const string &cpulist, T_all all_cpus)
{
    // "all" = use all available CPUs
    //  MK: imo, it would be nicer to use *, but that (along with most other
    //   special characters) has special meaning in YAML, and must be quoted
    if (cpulist == "all") return { all_cpus };
    return { cpulist };
}
template<typename T_all>
static cpu_set parse_cpu_set(const YAML::Node &node, T_all all_cpus)
{
    return parse_cpu_set<T_all>(node.as<string>(), all_cpus);
}

void Config::load_from_file(const string &file_name)
{
    try {
        config = YAML::LoadFile(file_name);
        config_is_regular_file = fs::is_regular_file(file_name);
        config_file_path = file_name;
    } catch (const YAML::BadFile &) {
        throw_with_nested(runtime_error("Cannot load configuration file: " + file_name));
    } catch (const YAML::Exception &e) {
        throw runtime_error("Configuration error: "s + e.what());
    }
}

void Config::load_from_string(const string &config_str)
{
    try {
        config = YAML::Load(config_str);
        config_file_path = nullopt;
    } catch (const YAML::Exception &e) {
        throw runtime_error("Configuration error: "s + e.what());
    }
}

static Node normalize_process(const Node &proc, float default_budget)
{
    Node norm_proc;
    if (proc.IsScalar()) {
        norm_proc["cmd"] = proc.as<string>();
    } else {
        for (const auto &key : proc) {
            auto k = key.first.as<string>();
            if (k == "cmd") {
                // which shell command to run to start the process
                norm_proc[k] = proc[k].as<string>();
            } else if (k == "budget" || k == "jitter") {
                // budget: how long can the process run in each window cycle
                // jitter: variance in budget for each window cycle
                // we're casting to int instead of unsigned int, as yaml-cpp error messages
                //  are quite bad, and so it's better if we check ourselves (below)
                norm_proc[k] = proc[k].as<int>();
            } else if (k == "init") {
                // if true, we wait until the process completes initialization
                //  before freezing it and starting normal scheduling
                norm_proc[k] = proc[k].as<bool>();
            } else if (k == "frequency") {
                norm_proc[k] = proc[k].as<double>();
            } else {
                throw runtime_error("Unexpected config key: " + k);
            }
        }
    }

    if (!norm_proc["budget"]) {
        if (isnan(default_budget)) {
            throw runtime_error("Missing budget in process definition");
        }
        norm_proc["budget"] = int(default_budget);
    }

    int budget = norm_proc["budget"].as<int>();
    if (budget <= 0) {
        throw runtime_error(
          "Process budget must be a positive time duration in milliseconds, got '" +
          to_string(budget) + "'");
    }

    if (!norm_proc["jitter"]) {
        norm_proc["jitter"] = int(0);
    } else {
        int jitter = norm_proc["jitter"].as<int>();
        if (jitter < 0) {
            throw runtime_error(
              "Jitter must be a non-negative time duration in milliseconds, got '" +
              to_string(jitter) + "'");
        }
        // budget +- (jitter/2)
        if (jitter > 2 * budget) {
            throw runtime_error("`budget - jitter / 2` must be >= 0, otherwise jitter "
                                "could result in a negative budget, got budget: '" +
                                to_string(budget) + " ms', jitter: '" + to_string(jitter) + " ms'");
        }
    }

    if (!norm_proc["init"]) {
        norm_proc["init"] = false;
    }

    return norm_proc;
}

static Node normalize_processes(const Node &processes, float total_budget)
{
    Node norm_processes;
    if (processes.IsSequence()) {
        for (const auto &proc : processes) {
            norm_processes.push_back(
              normalize_process(proc, total_budget / static_cast<float>(processes.size())));
        }
    } else {
        norm_processes.push_back(normalize_process(processes, total_budget));
    }
    return norm_processes;
}

Node Config::normalize_partition(
  const Node &part,
  float total_budget) // can be NAN to require explicit budgets in YAML file
{
    Node norm_part, processes;

    if (part.IsSequence()) { // seq. of processes
        processes = normalize_processes(part, total_budget);
    } else if (part.IsMap()) {
        for (const auto &key : part) {
            auto k = key.first.as<string>();
            if (k == "name")
                norm_part[k] = part[k].as<string>();
            else if (k == "processes")
                processes = normalize_processes(part[k], total_budget);
            else if (k == "cmd" || k == "budget" || k == "jitter" || k == "init" ||
                     k == "_a53_freq" || k == "_a72_freq")
                ;
            else
                throw runtime_error("Unexpected config key: " + k);
        }
    }

    if (!norm_part["name"]) {
        string name = "anonymous_" + to_string(anonymous_partition_counter++);
        norm_part["name"] = name;
    }

    // use a single process in place of a partition definition
    if (!norm_part["processes"]) {
        if (processes.IsNull()) {
            Node process;
            for (const string &key :
                 { "cmd", "budget", "jitter", "init", "_a53_freq", "_a72_freq" }) {
                if (part[key]) {
                    process[key] = part[key];
                }
            }
            processes.push_back(normalize_process(process, total_budget));
        }
        norm_part["processes"] = processes;
    }

    return norm_part;
}

string Config::process_xx_partition_and_get_name(const Node &part,
                                                 float total_budget,
                                                 Node &partitions) // out: partitions defind here
{
    if (part.IsScalar()) {
        // string in canonical configuration
        return part.as<string>();
    }

    Node norm_part = normalize_partition(part, total_budget);
    partitions.push_back(norm_part);
    return norm_part["name"].as<string>();
}

string Config::process_xx_processes_and_get_name(const Node &processes,
                                                 float total_budget,
                                                 Node &partitions) // out: partitions defind here
{
    Node part;
    part["processes"] = processes;
    Node norm_part = normalize_partition(part, total_budget);
    partitions.push_back(norm_part);
    return norm_part["name"].as<string>();
}

Node Config::normalize_slice(const Node &slice,
                             float win_length,
                             Node &partitions) // out: partitions defined in the slice
{
    Node norm_slice;

    for (const auto &key : slice) {
        auto k = key.first.as<string>();
        if (k == "cpu")
            norm_slice[k] = slice[k].as<string>();
        else if (k == "frequency")
            norm_slice[k] = slice[k].as<double>();
        else if (k == "sc_partition")
            // if process budget is not set, `0.6 * window length` is used as a default for SC
            //  partition; otherwise, SC partition would use up the whole budget, not leaving any
            //  space for BE partition
            norm_slice[k] =
              process_xx_partition_and_get_name(slice[k], win_length * 0.6f, partitions);
        else if (k == "be_partition")
            norm_slice[k] =
              process_xx_partition_and_get_name(slice[k], win_length * 1.0f, partitions);
        else if (k == "sc_processes")
            norm_slice["sc_partition"] =
              process_xx_processes_and_get_name(slice[k], win_length * 0.6f, partitions);
        else if (k == "be_processes")
            norm_slice["be_partition"] =
              process_xx_processes_and_get_name(slice[k], win_length * 1.0f, partitions);
        else
            throw runtime_error("Unexpected config key in slice definition: " + k);
    }

    if (!norm_slice["cpu"]) {
        throw runtime_error("Missing cpu set in slice definition (`cpu` property)");
    }
    // validate that the cpulist is in correct format
    parse_cpu_set(norm_slice["cpu"], 0);
    return norm_slice;
}

Node Config::normalize_window(const Node &win,  // in: window to normalize
                              Node &partitions) // out: partitions defined in windows
{
    Node norm_win;
    int win_length = win["length"].as<int>();

    if (win["slices"] && (win["sc_partition"] || win["be_partition"]))
        throw runtime_error("Cannot have both 'slices' and '*_partition' in window definition");
    if (win["slices"] && (win["cpu"]))
        throw runtime_error("Cannot have both 'slices' and 'cpu' in window definition");
    if (win["slices"] && (win["sc_processes"] || win["be_processes"]))
        throw runtime_error("Cannot have both 'slices' and '*_processes' in window definition");
    if ((win["sc_partition"] && win["sc_processes"]) ||
        (win["be_partition"] && win["be_processes"]))
        throw runtime_error("Cannot have both '*_partition' and '*_processes' in the same window");

    for (const auto &key : win) {
        auto k = key.first.as<string>();
        if (k == "length")
            norm_win[k] = win_length;
        else if (k == "slices") {
            Node slices;
            for (const auto &slice : win[k])
                slices.push_back(
                  normalize_slice(slice, static_cast<float>(win_length), partitions));
            norm_win[k] = slices;
        } else if (k == "cpu" || k == "sc_partition" || k == "be_partition" ||
                   k == "sc_processes" || k == "be_processes")
            ;
        else
            throw runtime_error("Unexpected config key in window definition: " + k);
    }

    if (win.size() == 1 && win["length"]) {
        // if a window only has `length` defined, keep it empty without creating a slice
        return norm_win;
    }

    if (!norm_win["slices"]) {
        // support inline slice definition inside window
        Node slice;
        // set first to keep a more readable order
        if (!win["cpu"]) slice["cpu"] = "all";
        for (const string &key :
             { "cpu", "sc_partition", "be_partition", "sc_processes", "be_processes" }) {
            if (win[key]) slice[key] = win[key];
        }
        norm_win["slices"].push_back(
          normalize_slice(slice, static_cast<float>(win_length), partitions));
    }
    return norm_win;
}

/**
 * Validates that all referenced partitions exist and slices don't have overlapping cpusets.
 *
 * It would be simpler to do the validation using created scheduler objects
 * rather than the YAML config, but I want the validation to take place even when
 * only dumping normalized config, and creating an intermediate struct-based config
 * format is probably currently too much work for too little benefit.
 */
void Config::validate_config()
{
    // check if all references to partition names are valid
    int window_i = -1;
    for (const auto &win : config["windows"]) {
        window_i++;
        int slice_i = -1;
        for (const auto &slice : win["slices"]) {
            slice_i++;
            for (const auto &part_key : { "sc_partition", "be_partition" }) {
                if (!slice[part_key]) continue;
                // FIXME: slow, create a std::set first for the partition names
                auto searched_name = slice[part_key].as<string>();
                for (const auto &part : config["partitions"]) {
                    if (searched_name == part["name"].as<string>()) {
                        goto found;
                    }
                }
                throw runtime_error("Reference to unknown partition '" + searched_name +
                                    "' in window #" + to_string(window_i) + ", slice #" +
                                    to_string(slice_i));
            found:;
            }
        }
    }

    // check if slices in each window have any CPU set overlaps
    window_i = -1;
    for (const auto &win : config["windows"]) {
        window_i++;
        cpu_set acc{};
        for (const auto &slice : win["slices"]) {
            auto cpu_str = slice["cpu"].as<string>();
            auto slice_cpu = parse_cpu_set(slice["cpu"], 0x0);
            // if a slice has `cpu: all`, there must not be any other slices in the window
            // else, check for overlap with previous slices
            if ((cpu_str == "all" && win["slices"].size() > 1) || acc & slice_cpu) {
                throw runtime_error("Slices in window #" + to_string(window_i) +
                                    " have overlapping CPU sets");
            }
            acc |= slice_cpu;
        }
    }
}

void Config::normalize()
{
    if (!config.IsMap()) {
        // this also catches cases like `demos-sched -C -d '...'`,
        //  where `-d` is interpreted as config string
        throw runtime_error("Config must be YAML mapping node");
    }

    // TODO: allow setting power policy in the config file

    // TODO: change `partitions` from list of definitions to a map from partition
    //  name to process list

    // clang-format off
    bool set_cwd = config["set_cwd"]
        ? config_file_path
            ? config["set_cwd"].as<bool>()
            : throw runtime_error("'set_cwd' cannot be used in inline config string")
        : config_file_path ? true : false;
    // clang-format on
    config.remove("set_cwd");

    if (set_cwd && config_file_path && !config_is_regular_file) {
        throw runtime_error("When config is passed through a FIFO or other special file type, "
                            "'set_cwd' must be set to 'false'");
    }

    string demos_cpu = config["demos_cpu"] ? config["demos_cpu"].as<string>() : "all";
    // validate that the cpulist is in correct format
    parse_cpu_set(demos_cpu, 0);
    config.remove("demos_cpu");

    Node norm_partitions;
    for (const auto &part : config["partitions"]) {
        norm_partitions.push_back(normalize_partition(part, NAN));
    }
    config.remove("partitions");

    Node norm_windows;
    for (const auto &win : config["windows"]) {
        norm_windows.push_back(normalize_window(win, norm_partitions));
    }
    config.remove("windows");

    for (const auto &node : config) {
        throw runtime_error("Unexpected config key: " + node.first.as<string>());
    }

    Node norm_config;
    norm_config["set_cwd"] = set_cwd;
    norm_config["demos_cpu"] = demos_cpu;
    norm_config["partitions"] = norm_partitions;
    norm_config["windows"] = norm_windows;

    config = norm_config;
    validate_config();
}

static Partition *find_partition(const string &name, Partitions &partitions)
{
    for (auto &p : partitions) {
        if (p.get_name() == name) return &p;
    }
    // already checked during config validation above
    throw runtime_error("not reachable");
}

// TODO: Move this out of Config to new DemosSched class
void Config::create_scheduler_objects(const CgroupConfig &c,
                                      cpu_set &demos_cpuset,
                                      Windows &windows,
                                      Partitions &partitions)
{
    bool ppf_warned = false;
    optional<filesystem::path> process_cwd{};
    if (config["set_cwd"].as<bool>()) {
        ASSERT(config_file_path != nullopt);
        process_cwd =
          (fs::current_path() / config_file_path.value().parent_path()).lexically_normal();
        logger->debug("Using '{}' as the working directory for all processes",
                      process_cwd->string());
    } else {
        logger->trace("Using current working directory for all processes");
    }

    for (const auto &ypart : config["partitions"]) {
        partitions.emplace_back(
          c.freezer_cg, c.cpuset_cg, c.unified_cg, ypart["name"].as<string>());
        for (const auto &yprocess : ypart["processes"]) {
            auto budget = chrono::milliseconds(yprocess["budget"].as<int>());
            auto budget_jitter = chrono::milliseconds(yprocess["jitter"].as<int>());
            auto req_freq = yprocess["frequency"]
                              ? std::optional(CpuFrequencyHz{ static_cast<uint64_t>(
                                  1000 * 1000 * yprocess["frequency"].as<double>()) })
                              : std::nullopt;
            if (req_freq && !c.power_policy.supports_per_process_frequencies() && !ppf_warned) {
                logger->warn("Per-processes frequency specified in the configuration, but not supported by the power policy.");
                ppf_warned = true;
            }
            partitions.back().add_process(c.loop,
                                          yprocess["cmd"].as<string>(),
                                          process_cwd,
                                          budget,
                                          budget_jitter,
                                          req_freq,
                                          yprocess["init"].as<bool>());
        }
    }

    logger->trace("Initialized partitions and processes");

    // Read current CPU affinity mask.
    cpu_set allowed_cpus;
    sched_getaffinity(0, allowed_cpus.size(), allowed_cpus.ptr());

    demos_cpuset = parse_cpu_set(config["demos_cpu"], allowed_cpus);

    for (const auto &ywindow : config["windows"]) {
        int length = ywindow["length"].as<int>();

        auto budget = chrono::milliseconds(length);
        Window &w = windows.emplace_back(c.loop, budget, c.power_policy);

        for (const auto &yslice : ywindow["slices"]) {
            Partition *sc_part_ptr = nullptr, *be_part_ptr = nullptr;
            if (yslice["sc_partition"]) {
                sc_part_ptr = find_partition(yslice["sc_partition"].as<string>(), partitions);
            }
            if (yslice["be_partition"]) {
                be_part_ptr = find_partition(yslice["be_partition"].as<string>(), partitions);
            }

            cpu_set cpus = parse_cpu_set(yslice["cpu"], allowed_cpus);

            if ((cpus & allowed_cpus).count() == 0) {
                logger->warn(
                  "Slice is supposed to run on CPU cores '{}', but DEmOS can only"
                  "\n\trun on cores '{}' (the remaining cores are either not present on the"
                  "\n\tcurrent system or DEmOS has restricted CPU affinity); the slice will"
                  "\n\trun on core '{}' instead (the lowest allowed CPU core)",
                  cpus.as_list(),
                  allowed_cpus.as_list(),
                  allowed_cpus.lowest());
                // FIXME: this may cause silent collisions even
                //  after we checked for overlaps during config validation
                cpus.zero();
                cpus.set(allowed_cpus.lowest());

            } else if ((cpus & allowed_cpus) != cpus) {
                logger->warn("Slice will run on CPU cores '{}' instead of '{}', as the remaining "
                             "cores are either not available on current system or DEmOS is not "
                             "allowed to run on them due to configured process CPU affinity",
                             (cpus & allowed_cpus).as_list(),
                             cpus.as_list());
                cpus &= allowed_cpus;
            }

            auto req_freq = yslice["frequency"]
                              ? std::optional(CpuFrequencyHz{ static_cast<uint64_t>(
                                  1000 * 1000 * yslice["frequency"].as<double>()) })
                              : std::nullopt;
            if (req_freq && !c.power_policy.supports_per_slice_frequencies() && !ppf_warned) {
                logger->warn("Per-slice frequency specified in the configuration, but not "
                             "supported by the power policy.");
                ppf_warned = true;
            }

            w.add_slice(sc_part_ptr, be_part_ptr, cpus, req_freq);
        }
    }

    c.power_policy.validate(windows);
}

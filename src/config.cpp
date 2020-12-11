#include "config.hpp"
#include "cpu_set.hpp"
#include "partition.hpp"
#include "window.hpp"
#include <cmath>
#include <exception>

using namespace std;
using namespace YAML;

void Config::loadFile(std::string file_name)
{
    try {
        config = YAML::LoadFile(file_name);
    } catch (const YAML::BadFile &e) {
        throw runtime_error("Cannot load configuration file: " + file_name);
    } catch (const YAML::Exception &e) {
        throw runtime_error("Configuration error: "s + e.what());
    }
}

void Config::loadStr(string cfg)
{
    try {
        config = YAML::Load(cfg);
    } catch (const YAML::Exception &e) {
        throw runtime_error("Configuration error: "s + e.what());
    }
}

Node Config::normalize_process(const Node &proc, float default_budget)
{
    Node norm_proc;
    if (proc.IsScalar()) {
        norm_proc["cmd"] = proc.as<string>();
    } else {
        for (auto key : proc) {
            string k = key.first.as<string>();
            if (k == "cmd") {
                norm_proc[k] = proc[k].as<string>();
            } else if (k == "budget") {
                norm_proc[k] = proc[k].as<int>();
            } else {
                throw runtime_error("Unexpected key: " + k);
            }
        }
    }

    if (!norm_proc["budget"]) {
        if (isnan(default_budget)) {
            throw runtime_error("Missing budget");
        }
        norm_proc["budget"] = default_budget;
    }

    return norm_proc;
}

Node Config::normalize_processes(const Node &processes, float total_budget)
{
    Node norm_processes;
    if (processes.IsSequence()) {
        for (const auto &proc : processes) {
            norm_processes.push_back(normalize_process(proc, total_budget / processes.size()));
        }
    } else {
        norm_processes.push_back(normalize_process(processes, total_budget));
    }
    return norm_processes;
}

Node Config::normalize_partition(const Node &part,
                                 float total_budget) // can be NAN to enforce budget
{
    Node norm_part, processes;

    if (part.IsSequence()) { // seq. of processes
        processes = normalize_processes(part, total_budget);
    } else if (part.IsMap()) {
        for (auto key : part) {
            string k = key.first.as<string>();
            if (k == "name")
                norm_part[k] = part[k].as<string>();
            else if (k == "processes")
                processes = normalize_processes(part[k], total_budget);
            else if (k == "cmd")
                ;
            else if (k == "budget")
                ;
            else
                throw runtime_error("Unexpected key: " + k);
        }
    }

    if (!norm_part["name"]) {
        string name = "anonymous_" + to_string(anonymous_partition_counter++);
        norm_part["name"] = name;
    }

    if (!norm_part["processes"]) {
        if (!processes) {
            Node process;
            process["cmd"] = part["cmd"];
            if (part["budget"]) {
                process["budget"] = part["budget"];
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

    for (auto key : slice) {
        string k = key.first.as<string>();
        if (k == "cpu")
            norm_slice[k] = slice[k].as<string>();
        else if (k == "sc_partition")
            // if process budget is not set, 0.6 * window length is used as a default for SC
            // partition otherwise, SC partition would use up the whole budget, not leaving any
            // space for BE partition
            norm_slice[k] =
              process_xx_partition_and_get_name(slice[k], win_length * 0.6, partitions);
        else if (k == "be_partition")
            norm_slice[k] =
              process_xx_partition_and_get_name(slice[k], win_length * 1.0, partitions);
        else if (k == "sc_processes")
            norm_slice["sc_partition"] =
              process_xx_processes_and_get_name(slice[k], win_length * 0.6, partitions);
        else if (k == "be_processes")
            norm_slice["be_partition"] =
              process_xx_processes_and_get_name(slice[k], win_length * 1.0, partitions);
        else
            throw runtime_error("Unexpected key: " + k);
    }
    return norm_slice;
}

Node Config::normalize_window(const Node &win,  // in: window to normalize
                              Node &partitions) // out: partitions defined in windows
{
    Node norm_win;
    int win_length = win["length"].as<int>();

    if (win["slices"] && (win["sc_partition"] || win["be_partition"]))
        throw runtime_error("Cannot have both 'slices' and '*_partition' in windows definition.");
    if (win["slices"] && (win["sc_processes"] || win["be_processes"]))
        throw runtime_error("Cannot have both 'slices' and '*_processes' in windows definition.");
    if ((win["sc_partition"] && win["sc_processes"]) ||
        (win["be_partition"] && win["be_processes"]))
        throw runtime_error("Cannot have both '*_partition' and '*_processes' in the same window.");

    for (auto key : win) {
        string k = key.first.as<string>();
        if (k == "length")
            norm_win[k] = win_length;
        else if (k == "slices") {
            Node slices;
            for (const auto &slice : win[k])
                slices.push_back(normalize_slice(slice, win_length, partitions));
            norm_win[k] = slices;
        } else if (k == "sc_partition")
            ;
        else if (k == "be_partition")
            ;
        else if (k == "sc_processes")
            ;
        else if (k == "be_processes")
            ;
        else
            throw runtime_error("Unexpected key: " + k);
    }

    if (!norm_win["slices"]) {
        Node slice;
        slice["cpu"] = "0-" + to_string(MAX_CPUS - 1);
        for (const string &key :
             { "sc_partition", "be_partition", "sc_processes", "be_processes" }) {
            if (win[key]) slice[key] = win[key];
        }
        norm_win["slices"].push_back(normalize_slice(slice, win_length, partitions));
    }
    return norm_win;
}

void Config::normalize()
{
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
        throw runtime_error("Unexpected key: " + node.first.as<string>());
    }

    Node norm_config;
    norm_config["partitions"] = norm_partitions;
    norm_config["windows"] = norm_windows;

    config = norm_config;
}

static Partition *find_partition(const string name, Partitions &partitions)
{
    for (auto &p : partitions) {
        if (p.get_name() == name) return &p;
    }
    throw runtime_error("Could not find partition: " + name);
}

// TODO: Move this out of Config to new DemosSched class
void Config::create_demos_objects(const CgroupConfig &c, Windows &windows, Partitions &partitions)
{
    for (auto ypart : config["partitions"]) {
        partitions.emplace_back(
          c.freezer_cg, c.cpuset_cg, c.unified_cg, ypart["name"].as<string>());
        for (auto yprocess : ypart["processes"]) {
            auto budget = chrono::milliseconds(yprocess["budget"].as<int>());
            partitions.back().add_process(c.loop, yprocess["cmd"].as<string>(), budget);
        }
    }

    // Read current CPU affinity mask.
    cpu_set allowed_cpus;
    sched_getaffinity(0, allowed_cpus.size(), allowed_cpus.ptr());

    for (auto ywindow : config["windows"]) {
        int length = ywindow["length"].as<int>();
        Slices slices;

        for (auto yslice : ywindow["slices"]) {
            Partition *sc_part_ptr = nullptr, *be_part_ptr = nullptr;
            if (yslice["sc_partition"]) {
                sc_part_ptr = find_partition(yslice["sc_partition"].as<string>(), partitions);
            }
            if (yslice["be_partition"]) {
                be_part_ptr = find_partition(yslice["be_partition"].as<string>(), partitions);
            }

            cpu_set cpus(yslice["cpu"].as<string>());
            if ((cpus & allowed_cpus) != cpus) {
                cerr << "Warning: Running on CPUs " << (cpus & allowed_cpus).as_list()
                     << " instead of " << cpus.as_list() << endl;
                cpus &= allowed_cpus;
            }
            slices.push_back(
              make_unique<Slice>(c.loop, c.start_time, sc_part_ptr, be_part_ptr, cpus));
        }

        auto budget = chrono::milliseconds(length);
        windows.push_back(make_unique<Window>(move(slices), budget));
    }
}


#include "config_parsing.hpp"
#include <chrono>

using namespace std;
using namespace YAML;

static int anonymous_partition_counter = 0;

static void find_partition_by_name(const string name,
                                   Node &norm_slice,
                                   Node &parent,
                                   Node &out_config,
                                   Node &in_config,
                                   const string key,
                                   int default_budget)
{
    const string part_key = key + "_partition";
    for (auto ypart : in_config["partitions"]) {
        if (name == ypart["name"].as<string>()) {
            Node norm_part;
            norm_part["name"] = ypart["name"];
            for (auto yproc : ypart["processes"]) {
                Node norm_proc;
                norm_proc["cmd"] = yproc["cmd"];

                // TODO default budget
                if (yproc["budget"])
                    norm_proc["budget"] = yproc["budget"];
                else
                    norm_proc["budget"] = default_budget;
                norm_part["processes"].push_back(norm_proc);
            }
            out_config["partitions"].push_back(norm_part);
            norm_slice[part_key] = parent[part_key];
            return;
        }
    }

    // if name not found, treat it as cmd
    const string part_name = "anonymous_" + to_string(anonymous_partition_counter++);
    Node part;
    part["name"] = part_name;
    Node proc;
    proc["cmd"] = name;
    proc["budget"] = default_budget;
    part["processes"].push_back(proc);
    out_config["partitions"].push_back(part);
    norm_slice[part_key] = part_name;
}

static void create_partition_from_window(Node &norm_slice,
                                         Node &parent,
                                         Node &out_c,
                                         const string key,
                                         int default_budget)
{
    const string part_key = key + "_partition";
    Node norm_part;
    const string name = "anonymous_" + to_string(anonymous_partition_counter++);
    norm_part["name"] = name;
    norm_slice[part_key] = name;
    for (auto yproc : parent[part_key]) {
        Node norm_proc;
        norm_proc["cmd"] = yproc["cmd"];
        if (norm_proc["budget"])
            norm_proc["budget"] = yproc["budget"];
        else
            norm_proc["budget"] = default_budget;
        norm_part["processes"].push_back(norm_proc);
    }
    out_c["partitions"].push_back(norm_part);
}

static void parse_partitions(Node &norm_slice, Node &parent, Node &out_c, Node &in_c, int w_length)
{
    // safety critical
    if (parent["sc_partition"]) {
        auto tmpn = parent["sc_partition"];
        if (tmpn.IsScalar()) {
            // partiton defined by name
            find_partition_by_name(
              tmpn.as<string>(), norm_slice, parent, out_c, in_c, "sc", int(0.6 * w_length));
        } else if (tmpn.IsSequence()) {
            // partition definition inside window
            create_partition_from_window(norm_slice, parent, out_c, "sc", int(0.6 * w_length));
        }
    }

    // best effort
    if (parent["be_partition"]) {
        auto tmpn = parent["be_partition"];
        if (tmpn.IsScalar()) {
            // partiton defined by name
            find_partition_by_name(
              tmpn.as<string>(), norm_slice, parent, out_c, in_c, "be", w_length);
        } else if (tmpn.IsSequence()) {
            // partition definition inside window
            create_partition_from_window(norm_slice, parent, out_c, "be", w_length);
        }
    }
}

void normalize_config(YAML::Node &in_c, YAML::Node &out_c)
{
    for (auto ywin : in_c["windows"]) {
        Node norm_win;
        norm_win["length"] = ywin["length"];
        int w_length = ywin["length"].as<int>();

        if (!ywin["slices"]) {
            Node norm_slice;
            norm_slice["cpu"] = "0-" + to_string(MAX_NPROC - 1);
            parse_partitions(norm_slice, ywin, out_c, in_c, w_length);
            norm_win["slices"].push_back(norm_slice);
        }
        for (auto yslice : ywin["slices"]) {
            Node norm_slice;
            norm_slice["cpu"] = yslice["cpu"];
            parse_partitions(norm_slice, yslice, out_c, in_c, w_length);
            norm_win["slices"].push_back(norm_slice);
        }
        out_c["windows"].push_back(norm_win);
    }
}

void parse_config(YAML::Node &config, CgroupConfig &c, Windows &windows, Partitions &partitions)
{
    //    for (auto ywindow : config["windows"]) {
    //        int length = ywindow["length"].as<int>();
    //        Slices slices;
    //        for (auto yslice : ywindow["slices"]) {
    //            string cpus = yslice["cpu"].as<string>();

    //        }
    //    }
}

// void create_partition(std::string &name,
//                      const std::string key,
//                      Partitions &partitions,
//                      Config &c,
//                      YAML::Node n,
//                      std::chrono::milliseconds &default_budget)
//{
//    partitions.emplace_back(c.freezer_cg, c.cpuset_cg, c.unified_cg, name);
//    for (auto yprocess : n[key]) {
//        auto budget = default_budget;
//        if (yprocess["budget"]) {
//            budget = chrono::milliseconds(yprocess["budget"].as<int>());
//        }
//        partitions.back().add_process(c.loop, yprocess["cmd"].as<string>(), budget);
//    }
//}

// Partition *find_partition_by_name(std::string name,
//                                  Config &c,
//                                  Partitions &partitions,
//                                  std::chrono::milliseconds &default_budget)
//{
//    // try to find in already parsed partitions
//    for (auto &p : partitions)
//        if (p.get_name() == name)
//            return &p;

//    // in not found, try to find in config file and add new partition
//    for (auto ypartition : c.config["partitions"]) {
//        if (ypartition["name"]) {
//            if (name == ypartition["name"].as<string>()) {
//                create_partition(name, "processes", partitions, c, ypartition, default_budget);
//                return &partitions.back();
//            }
//        }
//    }

//    return nullptr;
//}

// Partition *parse_partitions(const std::string key,
//                            std::chrono::milliseconds &default_budget,
//                            Partitions &partitions,
//                            YAML::Node &n,
//                            Config &c)
//{
//    Partition *part_ptr;
//    // try to find partition name in partitions
//    try {
//        part_ptr = find_partition_by_name(n[key].as<string>(), c, partitions, default_budget);
//        // if name not found in partitions, treat it as cmd
//        if (!part_ptr) {
//            string name = to_string(anonyme_partition_counter++);
//            partitions.emplace_back(c.freezer_cg, c.cpuset_cg, c.unified_cg, name);
//            partitions.back().add_process(c.loop, n[key].as<string>(), default_budget);
//            part_ptr = &partitions.back();
//        }
//    } catch (YAML::BadConversion e) {
//        // partition name not defined, try to treat it as array of cmds
//        try {
//            auto cmd_strings = n[key];
//            cmd_strings[0].as<string>();

//            string name = to_string(anonyme_partition_counter++);
//            partitions.emplace_back(c.freezer_cg, c.cpuset_cg, c.unified_cg, name);
//            for (auto cmd : cmd_strings) {
//                partitions.back().add_process(c.loop, cmd.as<string>(), default_budget);
//            }
//            part_ptr = &partitions.back();
//        } catch (YAML::BadConversion e) {
//            // try to find "cmd" in slice def.
//            string name = to_string(anonyme_partition_counter++);
//            create_partition(name, key, partitions, c, n, default_budget);
//            part_ptr = &partitions.back();
//        }
//    }
//    return part_ptr;
//}

// void add_partitions_to_slice(YAML::Node &n,
//                             Config &c,
//                             Slices &slices,
//                             Partitions &partitions,
//                             int length,
//                             std::string &cpus)
//{
//    Partition *sc_part_ptr = nullptr, *be_part_ptr = nullptr;
//    if (n["sc_partition"]) {
//        auto default_budget = chrono::milliseconds(int(0.6 * length));
//        sc_part_ptr = parse_partitions("sc_partition", default_budget, partitions, n, c);
//    }
//    if (n["be_partition"]) {
//        auto default_budget = chrono::milliseconds(length);
//        be_part_ptr = parse_partitions("be_partition", default_budget, partitions, n, c);
//    }

//    slices.push_back(make_unique<Slice>(c.loop, c.start_time, sc_part_ptr, be_part_ptr, cpus));
//}

// void parse_config(Config &c, Windows &windows, Partitions &partitions)
//{
//    for (auto ywindow : c.config["windows"]) {
//        // todo try
//        int length = ywindow["length"].as<int>();
//        Slices slices;

//        if (ywindow["slices"]) {
//            for (auto yslice : ywindow["slices"]) {
//                string cpus = yslice["cpu"].as<string>();
//                add_partitions_to_slice(yslice, c, slices, partitions, length, cpus);
//            }
//        } else { // if not "slices:"
//            string cpus = "0-" + to_string(MAX_NPROC - 1);
//            add_partitions_to_slice(ywindow, c, slices, partitions, length, cpus);
//        }

//        auto budget = chrono::milliseconds(length);
//        windows.push_back(make_unique<Window>(move(slices), budget));
//    }
//}

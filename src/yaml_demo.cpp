#include <iostream>
#include <list>
#include <yaml-cpp/node/detail/node.h>
#include <yaml-cpp/yaml.h>

//#include "majorframe.hpp"

// g++ -I/usr/local/include -L/usr/local/lib yaml_demo.cpp -lyaml-cpp

using namespace std;
using namespace YAML;

const string ALL_CPUS = "0-7";
static int anonymous_partition_counter = 0;

void find_partition_by_name(const string name,
                            Node &norm_slice,
                            Node &parent,
                            Node &out_config,
                            Node &in_config,
                            const string key)
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
                norm_proc["budget"] = yproc["budget"];
                norm_part["processes"].push_back(norm_proc);
            }
            out_config["partitions"].push_back(norm_part);
            norm_slice[part_key] = parent["part_key"];
        }
    }

    // if name not found, treat it as cmd
    const string part_name = "anonymous_" + to_string(anonymous_partition_counter++);
    Node part;
    part["name"] = part_name;
    Node proc;
    proc["cmd"] = name;
    proc["budget"] = "default_budget";
    part["processes"].push_back(proc);
    out_config["partitions"].push_back(part);
    norm_slice[part_key] = part_name;
}

void create_partition_from_window(Node &norm_slice, Node &parent, Node &out_c, const string key)
{
    const string part_key = key + "_partition";
    Node norm_part;
    const string name = "anonymous_" + to_string(anonymous_partition_counter++);
    norm_part["name"] = name;
    norm_slice[part_key] = name;
    for (auto yproc : parent[part_key]) {
        Node norm_proc;
        norm_proc["cmd"] = yproc["cmd"];
        // TODO default budget
        norm_proc["budget"] = yproc["budget"];
        norm_part["processes"].push_back(norm_proc);
    }
    out_c["partitions"].push_back(norm_part);
}

// void create_partition_from_array(Node &ns, Node &node, Node &norm_config, const string key)
//{
//    const string part_key = key + "_partition";
//    const string proc_key = key + "_processes";
//    const string name = "anonymous_" + to_string(anonymous_partition_counter++);

//    Node part;
//    part["name"] = name;
//    ns[part_key] = name;

//    // expected xx_processes to be array of strings
//    for (auto yproc : node[proc_key]) {
//        Node proc;
//        proc["cmd"] = yproc.as<string>();
//        // TODO add default budget
//        proc["budget"] = "TODO";
//        part["processes"].push_back(proc);
//    }
//    norm_config["partitions"].push_back(part);
//}

void parse_partitions(Node &norm_s, Node &parent, Node &out_c, Node &in_c)
{
    // safety critical
    if (parent["sc_partition"]) {
        auto tmpn = parent["sc_partition"];
        if (tmpn.IsScalar()) {
            // partiton defined by name
            find_partition_by_name(tmpn.as<string>(), norm_s, parent, out_c, in_c, "sc");
        } else if (tmpn.IsSequence()) {
            // partition definition inside window
            create_partition_from_window(norm_s, parent, out_c, "sc");
        }
    } /* else if (node["sc_processes"]) {
         // partition defined by array of commands
         create_partition_from_array(ns, node, norm_config, "sc");
     }*/

    // best effort
    if (parent["be_partition"]) {
        auto tmpn = parent["be_partition"];
        if (tmpn.IsScalar()) {
            // partiton defined by name
            find_partition_by_name(tmpn.as<string>(), norm_s, parent, out_c, in_c, "be");
        } else if (tmpn.IsSequence()) {
            // partition definition inside window
            create_partition_from_window(norm_s, parent, out_c, "be");
        }
    }
}

void normalize_config(YAML::Node &out_c, YAML::Node &in_c)
{
    for (auto ywin : in_c["windows"]) {
        Node norm_win;
        norm_win["length"] = ywin["length"];

        if (!ywin["slices"]) {
            Node norm_slice;
            norm_slice["cpu"] = ALL_CPUS;
            parse_partitions(norm_slice, ywin, out_c, in_c);
            norm_win["slices"].push_back(norm_slice);
        }
        for (auto yslice : ywin["slices"]) {
            Node norm_slice;
            norm_slice["cpu"] = yslice["cpu"];
            parse_partitions(norm_slice, yslice, out_c, in_c);
            norm_win["slices"].push_back(norm_slice);
        }
        out_c["windows"].push_back(norm_win);
    }
}

int main(int argc, char *argv[])
{
    Node config = YAML::LoadFile("../test_config/config.yaml");

    cerr << config << endl << endl;

    YAML::Node normalized_config;
    normalize_config(normalized_config, config);

    cerr << normalized_config << endl;

    //    for (YAML::const_iterator it = config["partitions"].begin(); it !=
    //    config["partitions"].end();
    //         ++it) {
    //        cout << (*it)["name"].as<string>() << endl;
    //        // create partition
    //        for (YAML::const_iterator jt = (*it)["processes"].begin(); jt !=
    //        (*it)["processes"].end();
    //             ++jt) {
    //            cout << (*jt)["cmd"].as<string>() << endl;
    //            // add process to partition
    //        }
    //    }
    //    cout << endl;

    //    for (YAML::const_iterator it = config["windows"].begin(); it != config["windows"].end();
    //    ++it) {
    //        cout << (*it)["length"].as<int>() << endl;
    //        // create window
    //        for (YAML::const_iterator jt = (*it)["slices"].begin(); jt != (*it)["slices"].end();
    //        ++jt) {
    //            if ((*jt)["sc_partition"])
    //                cout << (*jt)["sc_partition"].as<string>() << endl;
    //            if ((*jt)["be_partition"])
    //                cout << (*jt)["be_partition"].as<string>() << endl;
    //            // create slice
    //        }
    //    }

    return 0;
}

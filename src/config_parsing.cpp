
#include "config_parsing.hpp"
#include <chrono>

using namespace std;

int anonyme_partition_counter = 0;

void create_partition(std::string &name, const std::string key, Partitions &partitions, Config &c, YAML::Node n, std::chrono::milliseconds &default_budget)
{
    partitions.emplace_back(c.freezer_cg, c.cpuset_cg,
                            c.unified_cg, name);
    for (auto yprocess : n[key]){
        auto budget = default_budget;
        if (yprocess["budget"]){
            budget = chrono::milliseconds( yprocess["budget"].as<int>() );
        }
        partitions.back().add_process(
                    c.loop, yprocess["cmd"].as<string>(),
                    budget);
    }
}

Partition *find_partition_by_name(std::string name, Config &c, Partitions &partitions, std::chrono::milliseconds &default_budget)
{
    // try to find in already parsed partitions
    for (auto &p : partitions)
        if (p.get_name() == name)
            return &p;

    //in not found, try to find in config file and add new partition
    for (auto ypartition : c.config["partitions"]) {
        if ( ypartition["name"] ){
            if( name == ypartition["name"].as<string>() ){
                create_partition(name, "processes", partitions, c, ypartition, default_budget);
                return &partitions.back();
            }
        }
    }

    return nullptr;
}

void add_partitions_to_slice(YAML::Node &n, Config &c, Slices &slices, Partitions &partitions, int length, std::string &cpus)
{
    Partition *scpart_ptr = nullptr, *bepart_ptr = nullptr;
    if(n["sc_partition"]){
        auto default_budget = chrono::milliseconds( int(0.6*length) );
        try{
            scpart_ptr = find_partition_by_name(
                    n["sc_partition"].as<string>(),
                    c, partitions, default_budget);
        } catch (YAML::BadConversion e) {
            // partition name not defined, try to find "cmd" in slice def.
            string name = to_string(anonyme_partition_counter++);
            create_partition( name, "sc_partition", partitions, c, n, default_budget);
            scpart_ptr = &partitions.back();
        }
    }
    if(n["be_partition"]){
        auto default_budget = chrono::milliseconds( length );
        try{
            bepart_ptr = find_partition_by_name(
                    n["be_partition"].as<string>(),
                    c, partitions, default_budget);
        } catch (YAML::BadConversion e) {
            // partition name not defined, try to find "cmd" in slice def.
            string name = to_string(anonyme_partition_counter++);
            create_partition( name, "be_partition", partitions, c, n, default_budget);
            bepart_ptr = &partitions.back();
        }
    }
    slices.push_back(make_unique<Slice>(
                    c.loop, c.start_time,
                    scpart_ptr, bepart_ptr, cpus));
}

void parse_config(Config &c, Windows &windows, Partitions &partitions)
{
    for (auto ywindow : c.config["windows"]) {
        // todo try
        int length = ywindow["length"].as<int>();
        Slices slices;

        if(ywindow["slices"]){
            for (auto yslice : ywindow["slices"]){
                string cpus = yslice["cpu"].as<string>();
                add_partitions_to_slice(yslice, c, slices, partitions, length, cpus);
            }
        } else { // if not "slices:"
            string cpus = "0-" + to_string(MAX_NPROC-1  );
            add_partitions_to_slice(ywindow, c, slices, partitions, length, cpus);
        }

        auto budget = chrono::milliseconds( length );
        windows.push_back( make_unique<Window>(move(slices), budget) );
    }
}

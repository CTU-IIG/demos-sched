
#include "config_parsing.hpp"
#include <chrono>

using namespace std;

int anonyme_partition_counter = 0;

void add_partitions_to_slice(YAML::Node &n, Config &c, Slices &slices, Partitions &partitions, std::chrono::milliseconds &length, std::string &cpus)
{
    Partition *scpart_ptr = nullptr, *bepart_ptr = nullptr;
    if(n["sc_partition"]){
        scpart_ptr = find_partition_by_name(
                    n["sc_partition"].as<string>(),
                    c, partitions, length);
    }
    if(n["be_partition"]){
        bepart_ptr = find_partition_by_name(
                    n["be_partition"].as<string>(),
                    c, partitions, length);
    }
    slices.push_back(make_unique<Slice>(
                    c.loop, c.start_time,
                    scpart_ptr, bepart_ptr, cpus));
}

void parse_config(Config &c, Windows &windows, Partitions &partitions)
{
    for (auto ywindow : c.config["windows"]) {
        // todo try
        auto length = chrono::milliseconds(ywindow["length"].as<int>());
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
        windows.push_back( make_unique<Window>(move(slices), length) );
    }
}


Partition *find_partition_by_name(std::string name, Config &c, Partitions &partitions, std::chrono::milliseconds &default_budget)
{
    // try to find in partitions
    for (auto &p : partitions)
        if (p.get_name() == name)
            return &p;

    //in not found, try to find in config file and add new partition
    for (auto ypartition : c.config["partitions"]) {
        if ( ypartition["name"] ){
            if( name == ypartition["name"].as<string>() ){
                partitions.emplace_back(c.freezer_cg,
                                        c.cpuset_cg,
                                        c.unified_cg,
                                        name);
                for (auto yprocess : ypartition["processes"]){
                    // todo if not budget
                    partitions.back().add_process(
                                c.loop, yprocess["cmd"].as<string>(),
                                chrono::milliseconds(yprocess["budget"].as<int>()));
                }
                return &partitions.back();
            }
        }
    }

    return nullptr;
}

#include "majorframe.hpp"
#include <yaml-cpp/yaml.h>
#include <list>
#include <iostream>

using namespace std::chrono_literals;

void print_help()
{
    printf("help:\n"
            "-h\n"
            "\t print this message\n"
            "-g <NAME>\n"
            "\t name of cgroup with user access,\n"
            "\t need to create /sys/fs/cgroup/freezer/<NAME> and\n"
            "\t /sys/fs/cgroup/cpuset/<NAME> manually, then\n"
            "\t sudo chown -R <user> .../<NAME>\n"
            "\t if not set, \"my_cgroup\" is used\n"
            "TODO -c <FILE>\n"
            "\t path to configuration file\n");
}

using namespace std;

int main(int argc, char *argv[])
{
    std::string freezer_path = "/sys/fs/cgroup/freezer/my_cgroup";
    std::string cpuset_path = "/sys/fs/cgroup/cpuset/my_cgroup";
    std::string unified_path = "/sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup";

    auto start_time = chrono::steady_clock::now();

    ev::default_loop loop;

    try {
        YAML::Node config(YAML::LoadFile("../src/config.yaml"));

        Partition empty_part(freezer_path, cpuset_path, unified_path, "empty_part");
        Partitions partitions;

        for(YAML::const_iterator it = config["partitions"].begin();
            it != config["partitions"].end(); ++it)
        {
            // create partition
            partitions.emplace_back( freezer_path, cpuset_path, unified_path, (*it)["name"].as<string>() );

            for(YAML::const_iterator jt = (*it)["processes"].begin();
                jt != (*it)["processes"].end(); ++jt)
            {
                // add process to partition
                partitions.back().add_process(loop, (*jt)["cmd"].as<string>(),
                                    chrono::milliseconds((*jt)["budget"].as<int>()) );
            }
        }
        cerr<<"parsed "<<partitions.size()<<" partitions"<<endl;

        Windows windows;
        for(YAML::const_iterator it = config["windows"].begin();
            it != config["windows"].end(); ++it)
        {
            Slices slices;
            for(YAML::const_iterator jt = (*it)["slices"].begin();
                jt != (*it)["slices"].end(); ++jt)
            {
                Partition *sc_part_ptr = &empty_part;
                Partition *be_part_ptr = &empty_part;
                // find and add partitions according to their names
                if ((*jt)["sc_partition"]){
                    for(auto iter = partitions.begin(); iter != partitions.end(); iter++){
                        if( iter->get_name() == (*jt)["sc_partition"].as<string>() ){
                            sc_part_ptr = &(*iter);
                            break;
                        }
                    }
                }
                if ((*jt)["be_partition"]){
                    for(auto iter = partitions.begin(); iter != partitions.end(); iter++){
                        if( iter->get_name() == (*jt)["be_partition"].as<string>() ){
                            be_part_ptr = &(*iter);
                            break;
                        }
                    }
                }
                // create slice
                slices.emplace_back( loop, start_time, *sc_part_ptr, *be_part_ptr, (*jt)["cpu"].as<string>());
            }
            windows.emplace_back( slices, std::chrono::milliseconds((*it)["length"].as<int>()) );
        }
        cerr<<"parsed "<<windows.size()<<" windows"<<endl;

        MajorFrame mf(loop, start_time, windows);
        mf.start();


    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}

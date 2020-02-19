#include "majorframe.hpp"
#include <yaml-cpp/yaml.h>
#include <list>
#include <iostream>
#include <fstream>
#include "demossched.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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

void load_cgroup_paths(string &freezer_p, string &cpuset_p, string &unified_p)
{
    ifstream cgroup_f( "/proc/"+ to_string(getpid()) + "/cgroup");
    int num;
    string path;
    while (cgroup_f >> num >> path) {
        if( path.size() > 2 && path.find("::") != string::npos )
            unified_p = "/sys/fs/cgroup/unified" + path.substr(2);
        if( path.size() > 9 && path.find(":freezer:") != string::npos )
            freezer_p = "/sys/fs/cgroup/freezer" + path.substr(9);
        if( path.size() > 8 && path.find(":cpuset:") != string::npos )
            cpuset_p = "/sys/fs/cgroup/cpuset" + path.substr(8); // skip :cpuset:
    }

    // check access rights
    struct stat sb;
    bool ok = true;

    CHECK(stat(unified_p.c_str(), &sb));
    if( sb.st_uid != getuid() ){
        cerr<< "permission denied, need to add this shell to the user-delegated unified cgroup." << endl
            << "Run these commands:" << endl
            << "sudo mkdir /sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup" << endl
            << "sudo chown -R <user> /sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup" << endl
            << "sudo echo $$ > /sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup/cgroup.procs" << endl;
        ok = false;
    }

    CHECK(stat(freezer_p.c_str(), &sb));
    if( sb.st_uid != getuid() ){
        cerr<< "permission denied, need to add this shell to the user-delegated freezer cgroup." << endl
            << "Run these commands:" << endl
            << "sudo mkdir /sys/fs/cgroup/freezer/my_cgroup" << endl
            << "sudo chown -R <user> /sys/fs/cgroup/freezer/my_cgroup" << endl
            << "echo $$ > /sys/fs/cgroup/freezer/my_cgroup/cgroup.procs" << endl;
        ok = false;
    }

    ifstream clone_children_f( cpuset_p + "/cgroup.clone_children");
    int n;
    clone_children_f >> n;
    if( n != 1 ){
        cerr<< "Need to set clone_children flag in cpuset before creating new cpuset cgroup." << endl
            << "Run these commands:" << endl
            << "sudo echo 1 > /sys/fs/cgroup/cpuset/cgroup.clone_children" << endl;
        ok = false;
    }

    CHECK(stat(cpuset_p.c_str(), &sb));
    if( sb.st_uid != getuid() ){
        cerr<< "permission denied, need to add this shell to the user-delegated cpuset cgroup." << endl
            << "Run these commands:" << endl
            << "sudo mkdir /sys/fs/cgroup/cpuset/my_cgroup" << endl
            << "sudo chown -R <user> /sys/fs/cpuset/my_cgroup" << endl
            << "echo $$ > /sys/fs/cgroup/cpuset/my_cgroup/cgroup.procs" << endl;
        ok = false;
    }

    if( !ok )
        throw std::system_error(1, std::generic_category(), std::string(__PRETTY_FUNCTION__) + ": wrong cgroup settings");

}

int main(int argc, char *argv[])
{
//    std::string freezer_path = "/sys/fs/cgroup/freezer/my_cgroup";
//    std::string cpuset_path = "/sys/fs/cgroup/cpuset/my_cgroup";
//    std::string unified_path = "/sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup";
    string freezer_path, cpuset_path, unified_path;

    load_cgroup_paths(freezer_path, cpuset_path, unified_path);

    auto start_time = chrono::steady_clock::now();

    ev::default_loop loop;

    try {
        YAML::Node config(YAML::LoadFile("../src/config.yaml"));

        Partition empty_part(freezer_path, cpuset_path, unified_path, "empty_part");
        Partitions partitions;

        for(auto ypartition : config["partitions"])
        {
            // create partition
            partitions.emplace_back( freezer_path, cpuset_path, unified_path, ypartition["name"].as<string>() );

            for(auto yprocess : ypartition["processes"])
            {
                // add process to partition
                partitions.back().add_process(loop, yprocess["cmd"].as<string>(),
                                    chrono::milliseconds(yprocess["budget"].as<int>()) );
            }
        }
        cerr<<"parsed "<<partitions.size()<<" partitions"<<endl;

        Windows windows;
        list<Slices> all_slices( config["windows"].size() );
        auto s_it = all_slices.begin();
        for(YAML::const_iterator it = config["windows"].begin();
            it != config["windows"].end(); ++it)
        {
            //Slices slices;
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
                s_it->emplace_back( loop, start_time, *sc_part_ptr, *be_part_ptr, (*jt)["cpu"].as<string>());
            }
            windows.emplace_back(*s_it , std::chrono::milliseconds((*it)["length"].as<int>()) );
            s_it++;
        }
        cerr<<"parsed "<<windows.size()<<" windows"<<endl;

        MajorFrame mf(loop, start_time, windows);
        mf.start();

        loop.run();


    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}

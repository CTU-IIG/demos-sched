#include "majorframe.hpp"
#include <yaml-cpp/yaml.h>
#include <list>
#include <iostream>
#include <fstream>
#include "demossched.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>

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

Cgroup unified, freezer, cpuset;

void load_cgroup_paths(string &freezer_p, string &cpuset_p, string &unified_p)
{
    ifstream cgroup_f( "/proc/"+ to_string(getpid()) + "/cgroup");
    int num;
    string path;
    string cpuset_parent;
    const string demos_cg_name = "/demos";
    while (cgroup_f >> num >> path) {
        if( num == 0 )
            unified_p = "/sys/fs/cgroup/unified/" + path.substr(2)+ "/.." + demos_cg_name;
        if( path.find(":freezer:") == 0 )
            freezer_p = "/sys/fs/cgroup/freezer" + path.substr(9) + demos_cg_name;
        if( path.find(":cpuset:") == 0 ){
            cpuset_p = "/sys/fs/cgroup/cpuset" + path.substr(8) + demos_cg_name;
            cpuset_parent = "/sys/fs/cgroup/cpuset" + path.substr(8);
        }
    }

    // check access rights
    stringstream commands;
    //Cgroup unified;
    try {
        unified = Cgroup(unified_p);
    } catch (system_error &e) {
        switch (e.code().value()) {
        case EACCES:
        case EPERM: commands << "sudo mkdir " << unified_p << endl; break;
        default: throw;
        }
    }
    try {
        unified.add_process(getpid());
    } catch (system_error&) {
        commands << "sudo chown -R " << getuid() << " " << unified_p << endl;
        commands << "sudo echo $$ > " << unified_p + "/cgroup.procs" << endl;
    }

    //Cgroup freezer;
    try {
        freezer = Cgroup(freezer_p);
    } catch (system_error &e) {
        switch (e.code().value()) {
        case EACCES:
        case EPERM: commands << "sudo mkdir " << freezer_p << endl; break;
        default: throw;
        }
    }
    try {
        freezer.add_process(getpid());
    } catch(system_error&) {
        commands << "sudo chown -R " << getuid() << " " << freezer_p << endl;
    }

    //Cgroup cpuset;
    try {
        cpuset = Cgroup(cpuset_p);

        ifstream cpus_f(cpuset_parent + "/cpuset.cpus");
        string cpus;
        cpus_f >> cpus;
        ofstream(cpuset_p + "/cpuset.cpus") << cpus;

        ifstream mems_f(cpuset_parent + "/cpuset.mems");
        string mems;
        mems_f >> mems;
        ofstream(cpuset_p + "/cpuset.mems") << mems;

        ofstream(cpuset_p + "/cgroup.clone_children") << "1";

    } catch (system_error &e) {
        switch (e.code().value()) {
        case EACCES:
        case EPERM:
            //commands << "sudo echo 1 > " << cpuset_parent + "/cgroup.clone_children" << endl;
            commands << "sudo mkdir " << cpuset_p << endl;
            break;
        default: throw;
        }
    }
    try {
        cpuset.add_process(getpid());
    } catch(system_error& e) {
        commands << "sudo chown -R " << getuid() << " " << cpuset_p << endl;
        cerr<< e.what() <<endl;
    }


    if (!commands.str().empty()) {
        cerr << "Cannot create necessary cgroups. Run me as root or run the following commands:" << endl
             << commands.str();
        exit(1);
    }
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

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
#include <algorithm>

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

void load_cgroup_paths(Cgroup& unified, Cgroup& freezer, Cgroup& cpuset)
{
    ifstream cgroup_f( "/proc/"+ to_string(getpid()) + "/cgroup");
    int num;
    string path;
    string cpuset_parent;
    const string demos_cg_name = "/demos";
    string unified_p, freezer_p, cpuset_p;

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
        unified = Cgroup(unified_p, true);
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
        commands << "sudo echo " << getppid() << " > " << unified_p + "/cgroup.procs" << endl;
    }

    //Cgroup freezer;
    try {
        freezer = Cgroup(freezer_p, true);
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
        cpuset = Cgroup(cpuset_p, true);

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
    Cgroup unified_root, freezer_root, cpuset_root;

    load_cgroup_paths(unified_root, freezer_root, cpuset_root);

    auto start_time = chrono::steady_clock::now();

    ev::default_loop loop;

    try {
        YAML::Node config;
        string config_path = argc > 1 ? argv[1] : "../src/config.yaml";
        try {
            config = YAML::LoadFile(config_path);
        } catch (YAML::BadFile &e) {
            throw std::runtime_error("Cannot load configuration file: " + config_path);
        }

        Partition empty_part(freezer_root, cpuset_root, unified_root, "empty_part");
        Partitions partitions;

        for(auto ypartition : config["partitions"])
        {
            // create partition
            partitions.emplace_back( freezer_root, cpuset_root, unified_root, ypartition["name"].as<string>() );

            for(auto yprocess : ypartition["processes"])
            {
                // add process to partition
                partitions.back().add_process(loop, yprocess["cmd"].as<string>(),
                                    chrono::milliseconds(yprocess["budget"].as<int>()) );
            }
        }
        cerr<<"parsed "<<partitions.size()<<" partitions"<<endl;

        Windows windows;
        for(auto ywindow : config["windows"])
        {
            Slices slices;
            //Slices slices;
            for(auto yslice : ywindow["slices"])
            {
                Partition *sc_part_ptr = &empty_part;
                Partition *be_part_ptr = &empty_part;
                // find and add partitions according to their names
                if (yslice["sc_partition"]) {
                    sc_part_ptr = &*find_if(begin(partitions), end(partitions),
                         [&yslice](auto &p){return p.get_name() == yslice["sc_partition"].as<string>(); } );
                }
                if (yslice["be_partition"]){
                    be_part_ptr = &*find_if(begin(partitions), end(partitions),
                         [&yslice](auto &p){return p.get_name() == yslice["be_partition"].as<string>(); } );
                    }
                // create slice
                slices.push_back(make_unique<Slice>( loop, start_time, *sc_part_ptr, *be_part_ptr, yslice["cpu"].as<string>()));
            }
            windows.push_back(make_unique<Window>(move(slices), chrono::milliseconds(ywindow["length"].as<int>())));
        }
        cerr<<"parsed "<<windows.size()<<" windows"<<endl;

        MajorFrame mf(loop, start_time, move(windows));
        mf.start();

        loop.run();


    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}

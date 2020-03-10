#include "demossched.hpp"
#include "majorframe.hpp"
#include <fstream>
#include <iostream>
#include <list>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>

using namespace std;
using namespace std::chrono_literals;

string opt_demos_cg_name = "demos";

void print_help()
{
    cout <<
        "Usage: demos-sched -c <CONFIG_FILE> [-h] [-g <CGROUP_NAME>]\n"
        "  -c <CONFIG_FILE>   path to configuration file\n"
        "  -g <CGROUP_NAME>   name of root cgroups, default \"" << opt_demos_cg_name << "\"\n"
        "  -h                 print this message\n";
}

using namespace std;

void load_cgroup_paths(Cgroup &unified,
                       Cgroup &freezer,
                       Cgroup &cpuset,
                       const std::string demos_cg_name)
{
    ifstream cgroup_f("/proc/" + to_string(getpid()) + "/cgroup");
    int num;
    string path;
    string cpuset_parent;
    string unified_p, freezer_p, cpuset_p;

    while (cgroup_f >> num >> path) {
        if (num == 0)
            unified_p = "/sys/fs/cgroup/unified/" + path.substr(2) + "/../" + demos_cg_name;
        if (path.find(":freezer:") == 0)
            freezer_p = "/sys/fs/cgroup/freezer" + path.substr(9) + "/" + demos_cg_name;
        if (path.find(":cpuset:") == 0) {
            cpuset_p = "/sys/fs/cgroup/cpuset" + path.substr(8) + "/" + demos_cg_name;
            cpuset_parent = "/sys/fs/cgroup/cpuset" + path.substr(8);
        }
    }

    // check access rights
    stringstream commands, critical_msg;

    try {
        unified = Cgroup(unified_p, true);
    } catch (system_error &e) {
        switch (e.code().value()) {
            case EACCES:
            case EPERM:
                commands << "sudo mkdir " << unified_p << endl;
                break;
            case EROFS:
            case ENOENT:
                critical_msg << "mount -t cgroup2 none /sys/fs/cgroup/unified" << endl;
                break;
            default:
                throw;
        }
    }
    try {
        unified.add_process(getpid());
    } catch (system_error &) {
        commands << "sudo chown -R " << getuid() << " " << unified_p << endl;
        commands << "sudo echo " << getppid() << " > " << unified_p + "/cgroup.procs" << endl;
    }

    try {
        freezer = Cgroup(freezer_p, true);
    } catch (system_error &e) {
        switch (e.code().value()) {
            case EACCES:
            case EPERM:
                commands << "sudo mkdir " << freezer_p << endl;
                break;
            case EROFS:
            case ENOENT:
                critical_msg << "mount -t cgroup -o freezer none /sys/fs/cgroup/freezer" << endl;
                break;
            default:
                throw;
        }
    }
    try {
        freezer.add_process(getpid());
    } catch (system_error &) {
        commands << "sudo chown -R " << getuid() << " " << freezer_p << endl;
    }

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
                commands << "sudo mkdir " << cpuset_p << endl;
                break;
            case EROFS:
            case ENOENT:
                critical_msg << "mount -t cgroup -o cpuset none /sys/fs/cgroup/cpuset" << endl;
                break;
            default:
                throw;
        }
    }
    try {
        cpuset.add_process(getpid());
    } catch (system_error &e) {
        commands << "sudo chown -R " << getuid() << " " << cpuset_p << endl;
    }

    if (!critical_msg.str().empty()) {
        cerr << "There is no cgroup controller. Run following commands:"
             << endl
             << critical_msg.str()
             << "if it fails, check whether the controllers are available in the kernel"
             << endl
             << "zcat /proc/config.gz | grep -E CONFIG_CGROUP_FREEZER|CONFIG_CPUSETS"
             << endl;
        exit(1);
    }

    if (!commands.str().empty()) {
        cerr << "Cannot create necessary cgroups. Run me as root or run the "
                "following commands:"
             << endl
             << commands.str();
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    int opt;
    string config_file;
    while ((opt = getopt(argc, argv, "hg:c:")) != -1) {
        switch (opt) {
            case 'g':
                opt_demos_cg_name = optarg;
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'h':
                print_help();
                exit(0);
            default:
                print_help();
                exit(1);
        }
    }
    if (config_file.empty()) {
        print_help();
        exit(1);
    }

    Cgroup unified_root, freezer_root, cpuset_root;

    load_cgroup_paths(unified_root, freezer_root, cpuset_root, opt_demos_cg_name);

    auto start_time = chrono::steady_clock::now();

    ev::default_loop loop;

    try {
        YAML::Node config;
        try {
            config = YAML::LoadFile(config_file);
        } catch (YAML::BadFile &e) {
            throw runtime_error("Cannot load configuration file: " + config_file);
        }

        Partitions partitions;

        for (auto ypartition : config["partitions"]) {
            // create partition
            partitions.emplace_back(
              freezer_root, cpuset_root, unified_root, ypartition["name"].as<string>());

            for (auto yprocess : ypartition["processes"]) {
                // add process to partition
                partitions.back().add_process(loop,
                                              yprocess["cmd"].as<string>(),
                                              chrono::milliseconds(yprocess["budget"].as<int>()));
            }
        }
        cerr << "parsed " << partitions.size() << " partitions" << endl;

        Windows windows;
        for (auto ywindow : config["windows"]) {
            Slices slices;
            // Slices slices;
            for (auto yslice : ywindow["slices"]) {
                Partition *sc_part_ptr = nullptr;
                Partition *be_part_ptr = nullptr;
                // find and add partitions according to their names
                if (yslice["sc_partition"]) {
                    sc_part_ptr = &*find_if(begin(partitions), end(partitions), [&yslice](auto &p) {
                        return p.get_name() == yslice["sc_partition"].as<string>();
                    });
                }
                if (yslice["be_partition"]) {
                    be_part_ptr = &*find_if(begin(partitions), end(partitions), [&yslice](auto &p) {
                        return p.get_name() == yslice["be_partition"].as<string>();
                    });
                }
                // create slice
                slices.push_back(make_unique<Slice>(
                  loop, start_time, sc_part_ptr, be_part_ptr, yslice["cpu"].as<string>()));
            }
            windows.push_back(
              make_unique<Window>(move(slices), chrono::milliseconds(ywindow["length"].as<int>())));
        }
        cerr << "parsed " << windows.size() << " windows" << endl;

        MajorFrame mf(loop, start_time, move(windows));
        mf.start();

        // configure linux scheduler
        struct sched_param sp = {.sched_priority = 99};
        if( sched_setscheduler( 0, SCHED_FIFO, &sp ) == -1 )
            cerr << "Warning: running demos without rt priority, consider running as root" << endl;

        loop.run();

    } catch (const exception &e) {
        cerr << e.what() << endl;

    } catch (...) {
        cerr << "Unknown exception" << endl;
    }

    return 0;
}

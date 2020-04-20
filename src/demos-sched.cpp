#include "demossched.hpp"
#include "majorframe.hpp"
#include "config_parsing.hpp"
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
//size_t anonyme_partition_counter = 0;

void print_help()
{
    cout <<
        "Usage: demos-sched -c <CONFIG_FILE> [-h] [-g <CGROUP_NAME>]\n"
        "  -c <CONFIG_FILE>   path to configuration file\n"
        "  -C <CONFIG>        in-line configuration in YAML format\n"
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
    string config_file, config_str;
    while ((opt = getopt(argc, argv, "hg:c:C:")) != -1) {
        switch (opt) {
            case 'g':
                opt_demos_cg_name = optarg;
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'C':
                config_str = optarg;
                break;
            case 'h':
                print_help();
                exit(0);
            default:
                print_help();
                exit(1);
        }
    }
    if (config_file.empty() && config_str.empty()) {
        print_help();
        exit(1);
    }

    auto start_time = chrono::steady_clock::now();

    ev::default_loop loop;

    try {
        YAML::Node config;
        try {
            if (!config_file.empty()) {
                config = YAML::LoadFile(config_file);
            } else if (!config_str.empty()) {
                config = YAML::Load(config_str);
            }
        } catch (const YAML::BadFile &e) {
            throw runtime_error("Cannot load configuration file: " + config_file);
        } catch (const YAML::Exception &e) {
            throw runtime_error("Configuration error: "s + e.what());
        }

        Cgroup unified_root, freezer_root, cpuset_root;
        load_cgroup_paths(unified_root, freezer_root, cpuset_root, opt_demos_cg_name);

        Config c = {
            .config = config,
            .unified_cg = unified_root,
            .cpuset_cg = cpuset_root,
            .freezer_cg = freezer_root,
            .loop = loop,
            .start_time = start_time
        };
        Partitions partitions;
        Windows windows;

        parse_config(c, windows, partitions);

        cerr << "parsed " << partitions.size() << " partitions"
             << "and " << windows.size() << " windows" << endl;

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

#include "config.hpp"
#include "config_parsing.hpp"
#include "demossched.hpp"
#include "majorframe.hpp"
#include <fstream>
#include <iostream>
#include <list>

#include <algorithm>
#include <cerrno>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono_literals;

string opt_demos_cg_name = "demos";
// size_t anonyme_partition_counter = 0;

void print_help()
{
    cout << "Usage: demos-sched -c <CONFIG_FILE> [-h] [-g <CGROUP_NAME>]\n"
            "  -c <CONFIG_FILE>   path to configuration file\n"
            "  -C <CONFIG>        in-line configuration in YAML format\n"
            "  -g <CGROUP_NAME>   name of root cgroups, default \"" << opt_demos_cg_name << "\"\n"
            "  -s                 rerun itself via systemd-run to get access to unified cgroup hierarchy\n"
            "  -d                 dump config file without execution\n"
            "  -h                 print this message\n";
}

void handle_cgroup_exc(stringstream &commands,
                       stringstream &mount_cmds,
                       const system_error &e,
                       const string &sys_fs_cg_path)
{
    switch (e.code().value()) {
        case EACCES:
        case EPERM:
            commands << "sudo mkdir " << sys_fs_cg_path << endl;
            break;
        case EROFS:
        case ENOENT:
            if (sys_fs_cg_path.find("/sys/fs/cgroup/") != 0)
                throw "Unexpected cgroup path " + sys_fs_cg_path;
            if (sys_fs_cg_path.find("freezer/", 15) != string::npos) mount_cmds << "mount -t cgroup -o freezer none /sys/fs/cgroup/freezer";
            if (sys_fs_cg_path.find("cpuset/",  15) != string::npos) mount_cmds << "mount -t cgroup -o cpuset none /sys/fs/cgroup/cpuset";
            if (sys_fs_cg_path.find("unified/", 15) != string::npos) mount_cmds << "mount -t cgroup2 none /sys/fs/cgroup/unified";
            else throw "Unexpected cgroup controller path" + sys_fs_cg_path;
            mount_cmds << endl;
            break;
        default:
            throw;
    }
}

void create_toplevel_cgroups(Cgroup &unified,
                       Cgroup &freezer,
                       Cgroup &cpuset,
                       const std::string demos_cg_name)
{
    string unified_p, freezer_p, cpuset_p;
    string cpus, mems;

    // Get information about our current cgroups
    {
        int num;
        string path;
        ifstream cgroup_f("/proc/" + to_string(getpid()) + "/cgroup");

        while (cgroup_f >> num >> path) {
            if (num == 0)
                unified_p = "/sys/fs/cgroup/unified/" + path.substr(2) + "/../" + demos_cg_name;
            if (path.find(":freezer:") == 0)
                freezer_p = "/sys/fs/cgroup/freezer" + path.substr(9) + "/" + demos_cg_name;
            if (path.find(":cpuset:") == 0) {
                cpuset_p = "/sys/fs/cgroup/cpuset" + path.substr(8) + "/" + demos_cg_name;
                string cpuset_parent = "/sys/fs/cgroup/cpuset" + path.substr(8);
                ifstream(cpuset_parent + "/cpuset.cpus") >> cpus;
                ifstream(cpuset_parent + "/cpuset.mems") >> mems;
            }
        }
    }

    // check access rights
    stringstream commands, mount_cmds;

    try {
        unified = Cgroup(unified_p, true);
    } catch (system_error &e) {
        handle_cgroup_exc(commands, mount_cmds, e, unified_p);
    }
    try {
        unified.add_process(getpid());
    } catch (system_error &) {
        commands << "sudo chown -R " << getuid() << " " << unified_p << endl;

        // MS: Why is the below command needed for unified and not other controllers?
        commands << "sudo echo " << getppid() << " > " << unified_p + "/cgroup.procs" << endl;
    }

    try {
        freezer = Cgroup(freezer_p, true);
    } catch (system_error &e) {
        handle_cgroup_exc(commands, mount_cmds, e, freezer_p);
    }

    try {
        freezer.add_process(getpid());
    } catch (system_error &) {
        commands << "sudo chown -R " << getuid() << " " << freezer_p << endl;
    }

    try {
        cpuset = Cgroup(cpuset_p, true);
        ofstream(cpuset_p + "/cpuset.cpus") << cpus;
        ofstream(cpuset_p + "/cpuset.mems") << mems;
        ofstream(cpuset_p + "/cgroup.clone_children") << "1";
    } catch (system_error &e) {
        handle_cgroup_exc(commands, mount_cmds, e, cpuset_p);
    }
    try {
        cpuset.add_process(getpid());
    } catch (system_error &e) {
        commands << "sudo chown -R " << getuid() << " " << cpuset_p << endl;
    }

    if (!mount_cmds.str().empty()) {
        cerr << "There is no cgroup controller. Run following commands:" << endl
             << mount_cmds.str()
             << "if it fails, check whether the controllers are available in the kernel" << endl
             << "zcat /proc/config.gz | grep -E CONFIG_CGROUP_FREEZER|CONFIG_CPUSETS" << endl;
        exit(1);
    }

    if (!commands.str().empty()) {
        cerr << "Cannot create necessary cgroups. Run demos-sched as root or run the "
                "following commands:"
             << endl
             << commands.str();
        exit(1);
    }
}

string pluralize(int count, string noun)
{
    return to_string(count) + " " + noun + (count != 1 ? "s" : "");
}

void reexec_via_systemd_run(int argc, char *argv[], const Config &config)
{
    vector<const char*> args({
        "systemd-run", "--scope", "-p",  "Delegate=yes", "--user"
    });

    for (int i = 0; i < argc; i++)
        if (i == 0 || strcmp(argv[i], "-s") != 0)
            args.push_back(argv[i]);
    args.push_back(nullptr);

    for (const auto arg : args) {
        if (arg)
            cerr << arg << " ";
        else
            cerr << endl;
    }

    CHECK(execvp(args[0], const_cast<char**>(args.data())));
}

int main(int argc, char *argv[])
{
    int opt;
    string config_file, config_str;
    bool dump_config = false;
    bool systemd_run = false;

    while ((opt = getopt(argc, argv, "dhg:c:C:s")) != -1) {
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
            case 'd':
                dump_config = true;
                break;
            case 's':
                systemd_run = true;
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

    Config config;

    try {
        if (!config_file.empty()) {
            config.loadFile(config_file);
        } else if (!config_str.empty()) {
            config.loadStr(config_str);
        }

        config.normalize();

        if (dump_config) {
            cout << config.get() << endl;
            return 0;
        }

        if (systemd_run)
            reexec_via_systemd_run(argc, argv, config);

        Cgroup unified_root, freezer_root, cpuset_root;
        create_toplevel_cgroups(unified_root, freezer_root, cpuset_root, opt_demos_cg_name);

        CgroupConfig cc = { .unified_cg = unified_root,
                            .cpuset_cg = cpuset_root,
                            .freezer_cg = freezer_root,
                            .loop = loop,
                            .start_time = start_time };
        Partitions partitions;
        Windows windows;

        config.create_demos_objects(cc, windows, partitions);

        cerr << "Parsed " << pluralize(partitions.size(), "partition") << " and " << pluralize(windows.size(), "window") << endl;

        if (partitions.size() == 0 || windows.size() == 0)
            errx(1, "Need at least one partition in one window");

        MajorFrame mf(loop, start_time, move(windows));
        mf.start();

        // configure linux scheduler
        struct sched_param sp = { .sched_priority = 99 };
        if (sched_setscheduler(0, SCHED_FIFO, &sp) == -1)
            cerr << "Warning: running demos without rt priority, consider running as root" << endl;

        loop.run();

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "Unknown exception" << endl;
        return 1;
    }

    return 0;
}

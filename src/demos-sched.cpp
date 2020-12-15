#include "config.hpp"
#include "config_parsing.hpp"
#include "demossched.hpp"
#include "majorframe.hpp"
#include <fstream>
#include <iostream>
#include <list>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.hpp"
#include "spdlog/cfg/env.h"

using namespace std;
using namespace std::chrono_literals;

string opt_demos_cg_name = "demos";
// size_t anonyme_partition_counter = 0;

void print_help()
{
    // clang-format off
    cout << "Usage: demos-sched -c <CONFIG_FILE> [-h] [-g <CGROUP_NAME>]\n"
            "  -c <CONFIG_FILE>   path to configuration file\n"
            "  -C <CONFIG>        in-line configuration in YAML format\n"
            "  -g <CGROUP_NAME>   name of root cgroups, default \"" << opt_demos_cg_name << "\"\n"
            "  -s                 rerun itself via systemd-run to get access to unified cgroup hierarchy\n"
            "  -d                 dump config file without execution\n"
            "  -h                 print this message\n"
            "To control logger output, use the following environment variables:\n"
            "  DEMOS_PLAIN_LOG flag - if present, logs will not contain colors and time\n"
            "  SPDLOG_LEVEL=<level> (see https://spdlog.docsforge.com/v1.x/api/spdlog/cfg/helpers/load_levels/)";
    // clang-format on
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
            if (sys_fs_cg_path.find("/sys/fs/cgroup/") != 0) {
                throw "Unexpected cgroup path " + sys_fs_cg_path;
            }

            if (sys_fs_cg_path.find("freezer/", 15) != string::npos) {
                mount_cmds << "mount -t cgroup -o freezer none /sys/fs/cgroup/freezer";
            } else if (sys_fs_cg_path.find("cpuset/", 15) != string::npos) {
                mount_cmds << "mount -t cgroup -o cpuset none /sys/fs/cgroup/cpuset";
            } else if (sys_fs_cg_path.find("unified/", 15) != string::npos) {
                mount_cmds << "mount -t cgroup2 none /sys/fs/cgroup/unified";
            } else {
                throw "Unexpected cgroup controller path" + sys_fs_cg_path;
            }
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
    string unified_path, freezer_path, cpuset_path;
    string cpus, mems;

    // Get information about our current cgroups
    {
        int num;
        string path;
        ifstream cgroup_f("/proc/" + to_string(getpid()) + "/cgroup");

        while (cgroup_f >> num >> path) {
            if (num == 0) {
                unified_path = "/sys/fs/cgroup/unified" + path.substr(2) + "/" + demos_cg_name;
            }
            if (path.find(":freezer:") == 0) {
                freezer_path = "/sys/fs/cgroup/freezer" + path.substr(9) + "/" + demos_cg_name;
            }
            if (path.find(":cpuset:") == 0) {
                cpuset_path = "/sys/fs/cgroup/cpuset" + path.substr(8) + "/" + demos_cg_name;
                // Read settings from correct cpuset cgroup
                string cpuset_parent = "/sys/fs/cgroup/cpuset" + path.substr(8);
                ifstream(cpuset_parent + "/cpuset.cpus") >> cpus;
                ifstream(cpuset_parent + "/cpuset.mems") >> mems;
            }
        }
    }

    assert(!unified_path.empty());
    assert(!freezer_path.empty());
    assert(!cpuset_path.empty());

    logger->debug("Checking cgroup access permissions...");

    stringstream commands, mount_cmds;

    struct Child
    {
        const pid_t pid;
        Child()
            : pid(CHECK(fork()))
        {
            if (pid == 0) {
                // Dummy child for cgroup permission testing
                pause(); // wait for signal
                exit(0);
            }
        }
        ~Child() { kill(pid, SIGTERM); } // kill the process when going out of scope
    } child;

    // TODO: Verify that commands added to the commands variable still
    //  make sense (after changing a bit cgroup structure).

    try {
        unified = Cgroup(unified_path, true);
    } catch (system_error &e) {
        handle_cgroup_exc(commands, mount_cmds, e, unified_path);
    }
    try {
        unified.add_process(child.pid);
    } catch (system_error &) {
        commands << "sudo chown -R " << getuid() << " " << unified_path << endl;
        // MS: Why is the below command needed for unified and not other controllers?
        commands << "sudo echo " << getppid() << " > " << unified_path + "/cgroup.procs" << endl;
    }

    try {
        freezer = Cgroup(freezer_path, true);
    } catch (system_error &e) {
        handle_cgroup_exc(commands, mount_cmds, e, freezer_path);
    }
    try {
        freezer.add_process(child.pid);
    } catch (system_error &) {
        commands << "sudo chown -R " << getuid() << " " << freezer_path << endl;
    }

    try {
        cpuset = Cgroup(cpuset_path, true);
        ofstream(cpuset_path + "/cpuset.cpus") << cpus;
        ofstream(cpuset_path + "/cpuset.mems") << mems;
        ofstream(cpuset_path + "/cgroup.clone_children") << "1";
    } catch (system_error &e) {
        handle_cgroup_exc(commands, mount_cmds, e, cpuset_path);
    }
    try {
        cpuset.add_process(child.pid);
    } catch (system_error &e) {
        commands << "sudo chown -R " << getuid() << " " << cpuset_path << endl;
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

    logger->debug("Cgroup permission check passed");
}

string pluralize(int count, string noun)
{
    return to_string(count) + " " + noun + (count != 1 ? "s" : "");
}

void reexec_via_systemd_run(int argc, char *argv[])
{
    vector<const char *> args({ "systemd-run", "--scope", "-p", "Delegate=yes", "--user" });

    for (int i = 0; i < argc; i++) {
        if (i == 0 || strcmp(argv[i], "-s") != 0) {
            args.push_back(argv[i]);
        }
    }
    args.push_back(nullptr);

    for (const auto arg : args) {
        if (arg) {
            cerr << arg << " ";
        } else {
            cerr << endl;
        }
    }

    CHECK(execvp(args[0], const_cast<char **>(args.data())));
}

// TODO: after we figure out what else should this class contain,
//  it should probably be moved to separate file
class DemosScheduler
{
public:
    DemosScheduler(ev::loop_ref loop, MajorFrame &mf)
        : loop(loop)
        , mf(mf)
    {
        mf.set_completed_cb([&] { loop.break_loop(ev::ALL); });

        // setup signal handlers
        sigint.set<DemosScheduler, &DemosScheduler::signal_cb>(this);
        sigterm.set<DemosScheduler, &DemosScheduler::signal_cb>(this);
        sigint.start(SIGINT);
        sigterm.start(SIGTERM);
    }

    void run(std::function<void()> start_fn)
    {
        this->start_fn = start_fn;

        // we create a zero-length timer and run start_fn in it
        // that way, the function runs "inside" the event loop
        // and there isn't a special case for the first function (start_fn)
        // (otherwise, the first process was started before the event loop began)
        ev::timer immediate{ loop };
        immediate.set<DemosScheduler, &DemosScheduler::immediate_cb>(this);
        immediate.set(0., 0.);
        immediate.start();

        logger->debug("Starting event loop");
        loop.run();
        logger->debug("Event loop stopped");
    }

private:
    ev::loop_ref loop;
    MajorFrame &mf;
    ev::sig sigint{ loop };
    ev::sig sigterm{ loop };
    std::function<void()> start_fn = []{};

    void immediate_cb() {
        start_fn();
    }

    void signal_cb()
    {
        logger->info("Received stop signal (SIGTERM or SIGINT), stopping");
        mf.stop();
        loop.break_loop(ev::ALL);
    }
};

int main(int argc, char *argv[])
{
    int opt;
    string config_file, config_str;
    bool dump_config = false;
    bool systemd_run = false;

    while ((opt = getopt(argc, argv, "dhg:c:C:s")) != -1) {
        switch (opt) {
            case 'g': // custom root cgroup name
                opt_demos_cg_name = optarg;
                break;
            case 'c': // config file path
                config_file = optarg;
                break;
            case 'C': // inline YAML config string
                config_str = optarg;
                break;
            case 'd': // dump config file without execution
                dump_config = true;
                break;
            case 's': // rerun itself via systemd-run
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

    // don't reexec when only dumping config
    if (!dump_config && systemd_run) {
        reexec_via_systemd_run(argc, argv);
    }

    // Set loglevel from environment, e.g., export SPDLOG_LEVEL=info
    spdlog::cfg::load_env_levels();
    if (!getenv("DEMOS_PLAIN_LOG")) {
        logger->set_pattern(">>> %H:%M:%S.%e [%^%l%$] %v");
    } else {
        logger->set_pattern(">>> [%l] %v");
    }

    auto start_time = chrono::steady_clock::now();

    // demos is running in an event loop
    ev::default_loop loop;

    Config config;

    try {
        // load config
        if (!config_file.empty()) {
            config.loadFile(config_file);
        } else if (!config_str.empty()) {
            config.loadStr(config_str);
        } else {
            // no config was passed
            print_help();
            exit(1);
        }

        config.normalize();

        if (dump_config) {
            cout << config.get() << endl;
            exit(0);
        }

        // set up demos container cgroups
        Cgroup unified_root, freezer_root, cpuset_root;
        create_toplevel_cgroups(unified_root, freezer_root, cpuset_root, opt_demos_cg_name);

        logger->debug("Top level cgroups created");

        CgroupConfig cc = { .unified_cg = unified_root,
                            .cpuset_cg = cpuset_root,
                            .freezer_cg = freezer_root,
                            .loop = loop };
        Partitions partitions;
        Windows windows;

        config.create_demos_objects(cc, windows, partitions);

        logger->info("Parsed " + pluralize(partitions.size(), "partition") + " and " +
                     pluralize(windows.size(), "window"));

        if (partitions.size() == 0 || windows.size() == 0) {
            errx(1, "Need at least one partition in one window");
        }

        // set up all processes (in frozen state)
        for (auto &p : partitions) {
            p.exec_processes();
        }

        // configure linux scheduler - set highest possible priority for demos
        struct sched_param sp = { .sched_priority = 99 };
        if (sched_setscheduler(0, SCHED_FIFO, &sp) == -1) {
            logger->warn("Running demos without rt priority, consider running as root");
        }

        MajorFrame mf(loop, move(windows));
        DemosScheduler sched(loop, mf);

        // everything is set up now, start the event loop
        // the event loop terminates either on SIGTERM,
        //  SIGINT (Ctrl-c), or when all scheduled processes exit
        sched.run([&] { mf.start(start_time); });

        // clean up processes
        for (auto &p : partitions) {
            p.kill_all();
        }

    } catch (const exception &e) {
        logger->error("Exception: {}", e.what());
        exit(1);
    } catch (...) {
        logger->critical("Unknown exception");
        exit(1);
    }

    return 0;
}

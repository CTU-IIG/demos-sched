#include "config.hpp"
#include "demos_scheduler.hpp"
#include "lib/check_lib.hpp"
#include <iostream>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <power_policies/min_be.hpp>
#include <power_policies/none.hpp>
#include <sched.h>
#include <unistd.h>

using namespace std;

static string opt_demos_cg_name = "demos";

static void handle_cgroup_exc(stringstream &commands,
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
                throw runtime_error("Unexpected cgroup path " + sys_fs_cg_path);
            }

            if (sys_fs_cg_path.find("freezer/", 15) != string::npos) {
                mount_cmds << "mount -t cgroup -o freezer none /sys/fs/cgroup/freezer";
            } else if (sys_fs_cg_path.find("cpuset/", 15) != string::npos) {
                mount_cmds << "mount -t cgroup -o cpuset none /sys/fs/cgroup/cpuset";
            } else if (sys_fs_cg_path.find("unified/", 15) != string::npos) {
                mount_cmds << "mount -t cgroup2 none /sys/fs/cgroup/unified";
            } else {
                throw runtime_error("Unexpected cgroup controller path" + sys_fs_cg_path);
            }
            mount_cmds << endl;
            break;

        default:
            throw;
    }
}

static void create_toplevel_cgroups(Cgroup &unified,
                                    Cgroup &freezer,
                                    Cgroup &cpuset,
                                    const std::string &demos_cg_name)
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

    ASSERT(!unified_path.empty());
    ASSERT(!freezer_path.empty());
    ASSERT(!cpuset_path.empty());

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
}

static string pluralize(unsigned long count, const string &noun)
{
    return to_string(count) + " " + noun + (count != 1 ? "s" : "");
}

static void reexec_via_systemd_run(int argc, char *argv[])
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

// as this is user input parsing, I think it belongs here,
//  rather than as a static method on PowerPolicy
/** Constructs an instance of the selected power policy. */
static unique_ptr<PowerPolicy> get_power_policy(string policy_name)
{
    // convert to lowercase
    transform(policy_name.begin(), policy_name.end(), policy_name.begin(), ::tolower);

    if (policy_name.empty() || policy_name == "none") {
        logger->info(
          "Power management disabled (to enable, select power policy using -p parameter)");
        return make_unique<PowerPolicy_None>();
    }
    if (policy_name == "minbe") {
        return make_unique<PowerPolicy_MinBE>();
    }
    throw runtime_error("Unknown power policy selected: " + policy_name);
}

static void log_exception(const std::exception &e)
{
    logger->error("Exception: {}", e.what());
    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception &e) {
        log_exception(e);
    } catch (...) {
        logger->critical("Unknown nested exception");
    }
}

static void print_help()
{
    // clang-format off
    cout << "Usage: demos-sched -c <CONFIG_FILE> [-h] [-g <CGROUP_NAME>]\n"
            "  -c <CONFIG_FILE>    path to configuration file\n"
            "  -C <CONFIG>         inline configuration in YAML format\n"
            // TODO: list supported policies and short descriptions, probably in a separate function
            "  -p <POWER_POLICY>   name of selected power management policy; if multiple instances of DEmOS\n"
            "                       are running in parallel, this parameter must not be passed to more than a single one\n"
            "  -g <CGROUP_NAME>    name of root cgroups, default \"" << opt_demos_cg_name << "\"\n"
            "                       NOTE: this name must be unique for each running instance of DEmOS\n"
            "  -m <WINDOW_MESSAGE> print WINDOW_MESSAGE to stdout at the beginning of each window;\n"
            "                       this may be useful for external synchronization with scheduler windows\n"
            "  -M <MF_MESSAGE>     print MF_MESSAGE to stdout at the beginning of each major frame\n"
            "  -s                  rerun itself via systemd-run to get access to unified cgroup hierarchy\n"
            "  -d                  dump config file without execution\n"
            "  -h                  print this message\n"
            "To control logger output, use the following environment variables:\n"
            "  DEMOS_PLAIN_LOG flag - if present, logs will not contain colors and time\n"
            "  DEMOS_FORCE_COLOR_LOG flag - if present, logger will always print colored logs, \n"
            "      even when it is convinced that the attached terminal doesn't support it\n"
            "  SPDLOG_LEVEL=<level> (see https://spdlog.docsforge.com/v1.x/api/spdlog/cfg/helpers/load_levels/)\n";
    // clang-format on
}

int main(int argc, char *argv[])
{
    int opt;
    string config_file, config_str, window_sync_message, mf_sync_message, power_policy_name;
    bool dump_config = false;
    bool systemd_run = false;

    while ((opt = getopt(argc, argv, "c:C:p:g:m:M:sdh")) != -1) {
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
            case 'p': // selected power policy
                power_policy_name = optarg;
                break;
            case 'm': // window start sync message
                window_sync_message = optarg;
                break;
            case 'M': // major frame start sync message
                mf_sync_message = optarg;
                break;
            case 's': // rerun itself via systemd-run
                systemd_run = true;
                break;
            case 'd': // dump config file without execution
                dump_config = true;
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

    // setup our global spdlog logger (outputs to stderr)
    initialize_logger(getenv("DEMOS_PLAIN_LOG") ? ">>> [%l] %v" : ">>> %H:%M:%S.%e [%^%l%$] %v",
                      true,
                      getenv("DEMOS_FORCE_COLOR_LOG") != nullptr);

    try {
        // === CONFIG LOADING ======================================================================
        Config config;
        // load config
        if (!config_file.empty()) {
            config.load_from_file(config_file);
        } else if (!config_str.empty()) {
            config.load_from_string(config_str);
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


        // === ROOT CGROUP SETUP ===================================================================
        Cgroup unified_root, freezer_root, cpuset_root;
        create_toplevel_cgroups(unified_root, freezer_root, cpuset_root, opt_demos_cg_name);
        logger->debug("Top level cgroups created");


        // demos is running in a libev event loop
        ev::default_loop loop;
        // select power policy
        unique_ptr<PowerPolicy> pp = get_power_policy(power_policy_name);


        // === WINDOW & PARTITION INIT =============================================================
        CgroupConfig cc = { .unified_cg = unified_root,
                            .cpuset_cg = cpuset_root,
                            .freezer_cg = freezer_root,
                            .loop = loop,
                            .sched_events = *pp };
        Partitions partitions;
        Windows windows;

        // load windows and partitions from config
        config.create_scheduler_objects(cc, windows, partitions);

        logger->info("Parsed " + pluralize(partitions.size(), "partition") + " and " +
                     pluralize(windows.size(), "window"));

        if (partitions.empty() || windows.empty()) {
            throw runtime_error("Need at least one partition in one window");
        }


        // === SCHEDULER SETUP =====================================================================
        // initialize the main scheduler instance
        DemosScheduler sched(
          loop, move(partitions), move(windows), window_sync_message, mf_sync_message);
        // this spawns the underlying system processes
        sched.setup();

        // configure linux scheduler - set highest possible priority for demos
        // should be called after child process creation (in `sched.setup()`),
        //  as we don't want children to inherit RT priority
        struct sched_param sp = { .sched_priority = 99 };
        if (sched_setscheduler(0, SCHED_FIFO, &sp) == -1) {
            logger->warn("Running demos without rt priority, consider running as root");
        }

        // everything is set up now, start the event loop
        // the event loop terminates either on SIGTERM,
        //  SIGINT (Ctrl-c), or when all scheduled processes exit
        sched.run();

    } catch (const exception &e) {
        log_exception(e);
        exit(1);
    } catch (...) {
        logger->critical("Unknown exception");
        exit(1);
    }

    return 0;
}

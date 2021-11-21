#include "cgroup_setup.hpp"
#include "config.hpp"
#include "demos_scheduler.hpp"
#include "lib/assert.hpp"
#include "lib/check_lib.hpp"
#include "power_policy/_power_policy.hpp"
#include <sched.h>
#include "version.h"

using namespace std;

static string opt_demos_cg_name = "demos";

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
    cout << "Usage: demos-sched <OPTIONS>\n"
            "  -c <CONFIG_FILE>      path to configuration file\n"
            "  -C <CONFIG>           inline configuration in YAML format\n"
            // TODO: list supported policies and short descriptions, probably in a separate function
            "  [-p <POWER_POLICY>]   name and optional arguments of the selected power management policy;\n"
            "                         <POWER_POLICY> has the following form: <NAME>[:ARG1[,ARG2[,...]]]; \n"
            "                         if multiple instances of DEmOS are running in parallel, this parameter \n"
            "                         must not be passed to more than a single one\n"
            "  [-g <CGROUP_NAME>]    name of root cgroups, default \"" << opt_demos_cg_name << "\"\n"
            "                         NOTE: this name must be unique for each running instance of DEmOS\n"
            "  [-m <WINDOW_MESSAGE>] print WINDOW_MESSAGE to stdout at the beginning of each window;\n"
            "                         this may be useful for external synchronization with scheduler windows\n"
            "  [-M <MF_MESSAGE>]     print MF_MESSAGE to stdout at the beginning of each major frame\n"
            // TODO: shouldn't this be a config file option?
            "  [-t <TIMEOUT_MS>]     scheduler timeout (milliseconds); if all scheduled processes do not exit in\n"
            "                         this interval, DEmOS stops them and exits\n"
            "  [-s]                  rerun itself via systemd-run to get access to unified cgroup hierarchy\n"
            "  [-d]                  dump config file without execution\n"
            "  [-V]                  print demos-sched version and exit"
            "  [-h]                  print this message\n"
            "To control logger output, use the following environment variables:\n"
            "  SPDLOG_LEVEL=<level> (see https://spdlog.docsforge.com/v1.x/api/spdlog/cfg/helpers/load_levels/)\n"
            "    2 loggers are defined: 'main' and 'process'; to set a different log level for the process logger,\n"
            "    use e.g. 'SPDLOG_LEVEL=debug,process=info'\n"
            "  DEMOS_PLAIN_LOG flag - if present, logs will not contain colors and time\n"
            "  DEMOS_FORCE_COLOR_LOG flag - if present, logger will always print colored logs, \n"
            "    even when it is convinced that the attached terminal doesn't support it\n";
    // clang-format on
}

int main(int argc, char *argv[])
{
    int opt;
    string config_file, config_str, window_sync_message, mf_sync_message, power_policy_name;
    bool dump_config = false;
    bool systemd_run = false;
    std::optional<std::chrono::milliseconds> scheduler_timeout{};

    while ((opt = getopt(argc, argv, "c:C:p:g:m:M:t:sdhV")) != -1) {
        switch (opt) {
            case 'c': // config file path
                config_file = optarg;
                break;
            case 'C': // inline YAML config string
                config_str = optarg;
                break;
            case 'p': // selected power policy
                power_policy_name = optarg;
                break;
            case 'g': // custom root cgroup name
                opt_demos_cg_name = optarg;
                break;
            case 'm': // window start sync message
                window_sync_message = optarg;
                break;
            case 'M': // major frame start sync message
                mf_sync_message = optarg;
                break;
            case 't': // scheduler timeout
                scheduler_timeout = std::chrono::milliseconds{ std::stoll(optarg) };
                break;
            case 's': // rerun itself via systemd-run
                systemd_run = true;
                break;
            case 'd': // dump config file without execution
                dump_config = true;
                break;
            case 'V':
                fmt::print("demos-sched version {} ({})\n", GIT_VERSION, IF_DEBUG("debug", "release"));
                exit(0);
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
        // this print is useful to roughly measure startup times
        logger->debug("Starting DEmOS");
        RUN_DEBUG(TRACE("Compiled in debug mode"));
        logger->trace("Loading configuration");
        Config config;
        if (!config_file.empty()) {
            config.load_from_file(config_file);
        } else if (!config_str.empty()) {
            config.load_from_string(config_str);
        } else {
            // no config was passed
            print_help();
            exit(1);
        }

        logger->trace("Converting configuration to normal form");
        config.normalize();
        logger->debug("Configuration loaded");

        if (dump_config) {
            cout << config.get() << endl;
            exit(0);
        }


        // === ROOT CGROUP SETUP ===================================================================
        logger->trace("Creating top-level cgroups");
        Cgroup unified_root, freezer_root, cpuset_root;
        if (!cgroup_setup::create_toplevel_cgroups(
              unified_root, freezer_root, cpuset_root, opt_demos_cg_name)) {
            // cgroup creation failed
            return 1;
        }
        logger->debug("Top level cgroups created");


        // demos is running in a libev event loop
        ev::default_loop loop;
        // select power policy
        unique_ptr<PowerPolicy> pp = PowerPolicy::setup_power_policy(power_policy_name);


        // === WINDOW & PARTITION INIT =============================================================
        CgroupConfig cc = { .unified_cg = unified_root,
                            .cpuset_cg = cpuset_root,
                            .freezer_cg = freezer_root,
                            .loop = loop,
                            .power_policy = *pp };
        Partitions partitions;
        Windows windows;
        cpu_set demos_cpu; // cpuset DEmOS process itself runs on

        // load demos cpuset, windows and partitions from config
        config.create_scheduler_objects(cc, demos_cpu, windows, partitions);

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

        // configure linux scheduler - set the highest possible priority for demos
        // must be called after child process creation (in `sched.setup()`),
        //  as we don't want children to inherit RT priority
        struct sched_param sp = { .sched_priority = 99 };
        if (sched_setscheduler(0, SCHED_FIFO, &sp) == -1) {
            logger->warn("Running DEmOS without real-time priority, consider running as root");
        }

        // configure CPU affinity for DEmOS
        if (sched_setaffinity(0, demos_cpu.size(), demos_cpu.ptr()) == -1) {
            logger->warn("Failed to set the CPU affinity for DEmOS");
        }
        logger->debug("DEmOS is running on CPUs '{}'", demos_cpu.as_list());

        // TODO: document that DEmOS cannot reliably schedule under 1 millisecond,
        //  because libev doesn't guarantee it

        // everything is set up now, start the event loop
        // the event loop terminates either on timeout (if set),
        //  SIGTERM, SIGINT (Ctrl-c), or when all scheduled processes exit
        // FIXME: when a runtime error occurs, cgroups are not cleaned up, because
        //  they're not empty; catch the exception here, attempt to cleanup (sched.initiate_shutdown),
        //  then rethrow (if it fails, throw immediately)
        scheduler_timeout ? sched.run(scheduler_timeout.value()) : sched.run();

    } catch (const exception &e) {
        log_exception(e);
        exit(1);
    } catch (...) {
        logger->critical("Unknown exception");
        exit(1);
    }

    return 0;
}

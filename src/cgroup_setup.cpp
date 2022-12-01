#include "cgroup_setup.hpp"

#include "lib/assert.hpp"
#include "lib/check_lib.hpp"
#include <fstream>
#include <sstream>
#include <sys/statfs.h>
#include <linux/magic.h>

using namespace std;

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

[[nodiscard]] bool cgroup_setup::create_toplevel_cgroups(Cgroup &unified,
                                           Cgroup &freezer,
                                           Cgroup &cpuset,
                                           const std::string &demos_cg_name)
{
    string unified_path, freezer_path, cpuset_path;
    string cpus, mems;

    struct statfs sfs;
    CHECK(statfs("/sys/fs/cgroup", &sfs));

    bool cgroup_v2 = (sfs.f_type == CGROUP2_SUPER_MAGIC);

    // Get information about our current cgroups
    {
        int num;
        string path;
        ifstream cgroup_f("/proc/self/cgroup");

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
                ifstream(cpuset_parent + "/cpuset.effective_cpus") >> cpus;
                ifstream(cpuset_parent + "/cpuset.effective_mems") >> mems;
            }
        }
    }


    {
        // Check that we are in all needed cgroups
        auto paths_needed = std::vector({ make_pair(&unified_path, "unified") });
        if (!cgroup_v2)
            paths_needed.insert(paths_needed.end(),
                                {
                                  make_pair(&freezer_path, "freezer"),
                                  make_pair(&cpuset_path, "cpuset"),
                                });

        for (auto [path, type] : paths_needed) {
            if (path->empty()) throw runtime_error(string(type) + " cgroup not found");
        }
    }

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
    } catch (system_error &) {
        commands << "sudo chown -R " << getuid() << " " << cpuset_path << endl;
    }

    if (!mount_cmds.str().empty()) {
        cerr << "There is no cgroup controller. Run following commands:" << endl
             << mount_cmds.str()
             << "if it fails, check whether the controllers are available in the kernel" << endl
             << "zcat /proc/config.gz | grep -E CONFIG_CGROUP_FREEZER|CONFIG_CPUSETS" << endl;
        return false;
    }

    if (!commands.str().empty()) {
        cerr << "Cannot create necessary cgroups. Run demos-sched as root or run the "
                "following commands:"
             << endl
             << commands.str();
        return false;
    }

    return true;
}

#include "cgroup.hpp"
#include <err.h>
#include <fstream>

#include "demossched.hpp"

using namespace std;

Cgroup::Cgroup(string path, bool may_exist)
    : path(path)
{
#ifdef VERBOSE
    cerr << __PRETTY_FUNCTION__ << path << endl;
#endif
    try {
        CHECK_MSG(mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH), "mkdir " + path);
    } catch (system_error &e) {
        switch (e.code().value()) {
            case EEXIST:
                if (may_exist)
                    remove = false;
                break;
            default:
                throw;
        }
    }
}

Cgroup::Cgroup(string parent_path, string name)
    : Cgroup(parent_path + "/" + name)
{}

Cgroup::Cgroup(Cgroup &parent, string name)
    : Cgroup(parent.path, name)
{}

Cgroup::~Cgroup()
{
    if (path.empty() || !remove)
        return;
    int ret = rmdir(path.c_str());
    if (ret == -1)
        warn("rmdir %s", path.c_str());
}

void Cgroup::add_process(pid_t pid)
{
#ifdef VERBOSE
    cerr << __PRETTY_FUNCTION__ << path + "/cgroup.procs" << pid << endl;
#endif

    // ofstream( path + "/cgroup.procs" ) << pid; // WHY THIS DOESNT THROW?
    int fd = CHECK(open((path + "/cgroup.procs").c_str(), O_NONBLOCK | O_RDWR));
    string s = to_string(pid);
    CHECK(write(fd, s.c_str(), s.size()));
    close(fd);
}

void Cgroup::kill_all()
{
    ifstream procs(path + "/cgroup.procs");
    pid_t pid;
    while (procs >> pid) {
#ifdef VERBOSE
        std::cerr << "killing process " << pid << std::endl;
#endif
        CHECK(kill(pid, SIGKILL));
    }
}

std::string Cgroup::get_path() const
{
    return path;
}

//////////////////
CgroupFreezer::CgroupFreezer(string parent_path, string name)
    : Cgroup(parent_path, name)
{
    fd_state = CHECK(open((path + "/freezer.state").c_str(), O_RDWR | O_NONBLOCK));
}

CgroupFreezer::CgroupFreezer(Cgroup &parent, string name)
    : CgroupFreezer(parent.get_path(), name)
{}

void CgroupFreezer::freeze()
{
#ifdef VERBOSE
    std::cerr << __PRETTY_FUNCTION__ << " " + path << std::endl;
#endif
    const char buf[] = "FROZEN";
    CHECK(write(fd_state, buf, strlen(buf)));
}

void CgroupFreezer::unfreeze()
{
#ifdef VERBOSE
    std::cerr << __PRETTY_FUNCTION__ << " " + path << std::endl;
#endif
    const char buf[] = "THAWED";
    CHECK(write(fd_state, buf, strlen(buf)));
}

/////////////////////
CgroupCpuset::CgroupCpuset(string parent_path, string name)
    : Cgroup(parent_path, name)
{
    fd_cpus = CHECK(open((path + "/cpuset.cpus").c_str(), O_RDWR | O_NONBLOCK));
}

CgroupCpuset::CgroupCpuset(Cgroup &parent, std::string name /* Cgroup &garbage*/)
    : CgroupCpuset(parent.get_path(), name)
{}

void CgroupCpuset::set_cpus(cpu_set cpus)
{
    if (cpus != current_cpus) {
        string s = cpus.as_list();
        CHECK_MSG(write(fd_cpus, s.c_str(), s.size()), "Cannot set cpuset to " + s);
        current_cpus = cpus;
    }
}

CgroupEvents::CgroupEvents(ev::loop_ref loop,
                           string parent_path,
                           string name,
                           std::function<void(bool)> populated_cb)
    : Cgroup(parent_path, name)
    , events_w(loop)
    , populated_cb(populated_cb)
{
    fd_events = CHECK(open((path + "/cgroup.events").c_str(), O_RDONLY | O_NONBLOCK));

    events_w.set<CgroupEvents, &CgroupEvents::event_cb>(this);
    events_w.start(fd_events, ev::EXCEPTION);
}

CgroupEvents::CgroupEvents(ev::loop_ref loop,
                           CgroupEvents &parent,
                           string name,
                           std::function<void(bool)> populated_cb)
    : CgroupEvents(loop, parent.path, name, populated_cb)
{}

CgroupEvents::CgroupEvents(ev::loop_ref loop,
                           Cgroup &parent,
                           string name,
                           std::function<void(bool)> populated_cb)
    : CgroupEvents(loop, parent.get_path(), name, populated_cb)
{}

CgroupEvents::~CgroupEvents()
{
    events_w.stop();
}

void CgroupEvents::event_cb(ev::io &w, int revents)
{
    char buf[100];
    ssize_t size = CHECK(pread(w.fd, buf, sizeof(buf) - 1, 0));
    bool populated = (std::string(buf, size).find("populated 1") != std::string::npos);
    populated_cb(populated);
}

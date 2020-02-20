#include "cgroup.hpp"
#include <fstream>

#include "demossched.hpp"

using namespace std;

Cgroup::Cgroup(string path)
    : path(path)
{
    cerr << __PRETTY_FUNCTION__ << path << endl;
    try{
        CHECK_MSG( mkdir( path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH), "mkdir "+path );
    } catch (system_error &e) {
        switch (e.code().value()) {
        case EEXIST: /* ignore */; break;
        default: throw;
    }
}
}

Cgroup::Cgroup(string parent_path, string name)
    : Cgroup(parent_path + "/" /*+ to_string(cgrp_count) + "_"*/ + name)
{}

Cgroup::Cgroup(Cgroup &parent, string name)
    : Cgroup(parent.path, name)
{
    //parent.children.push_back(this);
}

Cgroup::~Cgroup()
{
    if (path.empty())
        return;
    try{
        CHECK_MSG( rmdir( path.c_str()), "rmdir "+path   );
    } catch (const system_error& e) {
        switch (e.code().value()) {
        case EACCES:
        case EPERM: /* ignore */; break;
        default: throw;
        }
    }
}

void Cgroup::add_process(pid_t pid)
{
    cerr<<__PRETTY_FUNCTION__<< path + "/cgroup.procs" << pid <<endl;

    //ofstream( path + "/cgroup.procs" ) << pid; // WHY THIS DOESNT THROW?
    int fd = CHECK( open( (path + "/cgroup.procs").c_str(), O_NONBLOCK|O_RDWR) );
    string s = to_string(pid);
    CHECK( write(fd, s.c_str(), s.size()) );
    close(fd);
}

void Cgroup::kill_all()
{
    ifstream procs( path + "/cgroup.procs");
    pid_t pid;
    while (procs >> pid) {
#ifdef VERBOSE
        std::cerr<<"killing process "<<pid<<std::endl;
#endif
        CHECK( kill(pid, SIGKILL) );
    }
}

std::string Cgroup::getPath() const
{
    return path;
}

//////////////////
CgroupFreezer::CgroupFreezer(string parent_path, string name)
    : Cgroup(parent_path, name)
{
    fd_state = CHECK( open( (path + "/freezer.state").c_str(), O_RDWR | O_NONBLOCK ) );
}

CgroupFreezer::CgroupFreezer(Cgroup &parent, string name)
    : CgroupFreezer(parent.getPath(), name)
{
    //parent.children.push_back(this);
}

void CgroupFreezer::freeze()
{
#ifdef VERBOSE
    std::cerr<< __PRETTY_FUNCTION__ << " " + path << std::endl;
#endif
    const char buf[] = "FROZEN";
    CHECK( write(fd_state, buf, strlen(buf)) );
}

void CgroupFreezer::unfreeze()
{
#ifdef VERBOSE
    std::cerr<< __PRETTY_FUNCTION__ << " " + path << std::endl;
#endif
    const char buf[] = "THAWED";
    CHECK( write(fd_state, buf, strlen(buf)) );
}

/////////////////////
CgroupCpuset::CgroupCpuset(string parent_path, string name)
    : Cgroup(parent_path, name)
{
    fd_cpus = CHECK( open( (path + "/cpuset.cpus").c_str(), O_RDWR | O_NONBLOCK ) );
}

CgroupCpuset::CgroupCpuset(Cgroup &parent, std::string name /* Cgroup &garbage*/)
    : CgroupCpuset(parent.getPath(), name)
{
    //parent.children.push_back(this);
}

void CgroupCpuset::set_cpus(string cpus)
{
    CHECK( write(fd_cpus, cpus.c_str(), cpus.size()) );
}

CgroupEvents::CgroupEvents(ev::loop_ref loop, string parent_path, string name, std::function<void (bool)> populated_cb)
    : Cgroup(parent_path, name)
    , events_w(loop)
    , populated_cb(populated_cb)
{
    fd_events = CHECK( open( (path + "/cgroup.events").c_str(), O_RDONLY | O_NONBLOCK ) );

    events_w.set<CgroupEvents, &CgroupEvents::clean_cb>(this);
    events_w.start(fd_events, ev::EXCEPTION);
}

CgroupEvents::CgroupEvents(ev::loop_ref loop, CgroupEvents &parent, string name, std::function<void (bool)> populated_cb)
    : CgroupEvents(loop, parent.path, name, populated_cb)
{
    //parent.children.push_back(this);
}

CgroupEvents::CgroupEvents(ev::loop_ref loop, Cgroup &parent, string name, std::function<void (bool)> populated_cb)
    : CgroupEvents(loop, parent.getPath(), name, populated_cb)
{
}

CgroupEvents::~CgroupEvents()
{
    events_w.stop();
}

void CgroupEvents::clean_cb(ev::io &w, int revents) {
    char buf[100];
    CHECK( pread(w.fd, buf, sizeof (buf) - 1, 0) );
    bool populated = (std::string(buf).find("populated 1") != std::string::npos);
    populated_cb(populated);
}

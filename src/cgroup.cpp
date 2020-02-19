#include "cgroup.hpp"
#include <fstream>

#include "demossched.hpp"

using namespace std;

int Cgroup::cgrp_count = 0;

Cgroup::Cgroup(string path)
    : path(path)
{
    cerr << __PRETTY_FUNCTION__ << path << endl;
    CHECK_MSG( mkdir( path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH), path );
    cgrp_count++;
}

Cgroup::Cgroup(string parent_path, string name)
    : Cgroup(parent_path + "/" /*+ to_string(cgrp_count) + "_"*/ + name)
{}

Cgroup::Cgroup(Cgroup &parent, string name)
    : Cgroup(parent.path, name)
{
    parent.children.push_back(this);
}

Cgroup::~Cgroup()
{
    if (path.empty())
        return;
    try{
        //cerr<< __PRETTY_FUNCTION__ << " " << path << endl;
        CHECK( rmdir( path.c_str()) );
    } catch (const std::exception& e) {
        throw e;
    }
}

void Cgroup::add_process(pid_t pid)
{
    ofstream( path + "/cgroup.procs" ) << pid;
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

CgroupFreezer::CgroupFreezer(CgroupFreezer &parent, string name)
    : CgroupFreezer(parent.path, name)
{
    parent.children.push_back(this);
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

CgroupCpuset::CgroupCpuset(CgroupCpuset &parent, std::string name /* Cgroup &garbage*/)
    : CgroupCpuset(parent.path, name)
{
    parent.children.push_back(this);
}

void CgroupCpuset::set_cpus(string cpus)
{
    CHECK( write(fd_cpus, cpus.c_str(), cpus.size()) );
}

////////////////
CgroupEvents::CgroupEvents(string parent_path, string name)
    : Cgroup(parent_path, name)
{
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
    parent.children.push_back(this);
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

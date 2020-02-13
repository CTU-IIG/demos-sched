#include "cgroup.hpp"
#include <fstream>

#include "demossched.hpp"

using namespace std;

int Cgroup::cgrp_count = 0;

Cgroup::Cgroup(string parent_path, string name)
    :path(parent_path + "/" + name)
{
    CHECK( mkdir( path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) );
    cgrp_count++;
}

Cgroup::Cgroup(Cgroup &parent, string name)
    : Cgroup(parent.path, name)
{
    parent.children.push_back(this);
}

Cgroup::~Cgroup()
{
    try{
        cerr<< __PRETTY_FUNCTION__ << " " << path << endl;
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

void CgroupEvents::clean_cb(ev::io &w, int revents) {
    cerr << __PRETTY_FUNCTION__ <<endl;
    char buf[100];
    CHECK( pread(w.fd, buf, sizeof (buf) - 1, 0) );
    bool populated = (std::string(buf).find("populated 1") != std::string::npos);
    populated_cb(populated);
}


//////////////////
//void create_cgroup(std::string path)
//{
//    CHECK( mkdir( path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) );
//}

//int Cgroup::open_fd(std::string path, int attr = O_RDWR | O_NONBLOCK )
//{
//    return CHECK( open( path.c_str(), attr) );
//}

//Cgroup::Cgroup(ev::loop_ref loop,
//               bool is_process_cgrp,
//               std::string cgrp_name,
//               int fd_cpuset_procs,
//               std::string partition_cgroup_name)
//    : name( partition_cgroup_name + std::to_string(cgrp_count) + "_"
//            + cgrp_name + "/")
//    , fd_cpuset_procs( fd_cpuset_procs )
//    , freezer_p(freezer_path + name)
//    , cpuset_p(cpuset_path + name)
//    , unified_p(unified_path + name)
//    , procs_w(loop)
//    , is_process_cgrp(is_process_cgrp)
//    , loop(loop)
//{
//    std::cerr<< __PRETTY_FUNCTION__ << "@" << this << " " << freezer_p <<std::endl;

//    try{
//        // create new freezer cgroup
//        create_cgroup( freezer_p);
//        // create unified cgroup for .events
//        create_cgroup( unified_p);
//        // open file descriptor for (un)freezing cgroup
//        fd_freezer_state = open_fd( freezer_p + "freezer.state");
//        // open file descriptor for processes in unified cgroup
//        fd_uni_procs = open_fd( unified_p + "cgroup.procs");
//        // open file descriptor for montoring cleanup
//        fd_uni_events = open_fd( unified_p + "cgroup.events", O_RDONLY);

//        // assign clean_cb just for processes
//        // partition cgroup delete manually for right order of deleting
//        if(is_process_cgrp){
//            procs_w.set<Cgroup, &Cgroup::clean_cb>(this);
//            procs_w.start(fd_uni_events, ev::EXCEPTION);
//        } else {
//            // create new cpuset cgroup
//            create_cgroup( cpuset_p);
//            // open file descriptor for processes in cpuset
//            this->fd_cpuset_procs = open_fd( cpuset_p + "cgroup.procs");
//            // open file descriptor for cpuset
//            fd_cpuset_cpus = open_fd( cpuset_p + "cpuset.cpus");
//        }
//        cgrp_count++;
//    } catch (const std::exception& e) {
//        std::cerr << "probably bad cgroup setting?" << std::endl;
//        delete_cgroup();
//        close_all_fd();
//        throw e;
//    }

//}

//void Cgroup::delete_cgroup()
//{
//    // delete cgroups
//    CHECK( rmdir( freezer_p.c_str()) );
//    CHECK( rmdir( unified_p.c_str()) );
//    if( !is_process_cgrp )
//        CHECK( rmdir( cpuset_p.c_str()) );
//    populated = false;
//    deleted = true;
//}

//void Cgroup::kill_all()
//{
//    if( !populated || killed )
//        return;
//    freeze();

//    // read pids from fd

//    ifstream procs(freezer_p + "cgroup.procs");
//    pid_t pid;
//    while (procs >> pid) {
//#ifdef VERBOSE
//        std::cerr<<"killing process "<<pid<<std::endl;
//#endif
//        CHECK( kill(pid, SIGKILL) );
//    }
//    killed = true;

//    unfreeze();
//}

//Cgroup::~Cgroup()
//{
//#ifdef VERBOSE
//    std::cerr<< __PRETTY_FUNCTION__ << " PID:"+std::to_string(getpid())+" @" << this << " " << freezer_p <<std::endl;
//#endif
//    if(!deleted){
//        if(is_process_cgrp){
//            kill_all();
//            loop.run();
//        }
//        delete_cgroup();
//    }
//    close_all_fd();
//}

//void Cgroup::clean_cb (ev::io &w, int revents)
//{
//    char buf[100];
//    CHECK( pread(w.fd, buf, sizeof (buf) - 1, 0) );

//    bool populated = (std::string(buf).find("populated 1") != std::string::npos);
//    if(!populated){
//#ifdef VERBOSE
//        std::cerr<< __PRETTY_FUNCTION__ << " " << freezer_p <<std::endl;
//#endif
//        w.stop();
//        delete_cgroup();
//    }

////    w.stop();
////    bool populate_new = ...;
////    if (!populated && populate_new)
////       populate_cnt++;
////    else if (populated && !populate_new) {
////        poplated_cnt--;
////        if (populted_cnt == 0)
////            loop.break();
////    }
//    // demos_sch.cgroup_clean(this);
//}

//void Cgroup::write_pid(pid_t pid, int fd)
//{
//    // add process to cgroup (echo PID > cgroup.procs)
//    char buf[20];
//    int size = sprintf(buf, "%d", pid);
//    CHECK(write(fd, buf, size+1));
//}



//void Cgroup::add_process(pid_t pid)
//{
//    try{
//        ofstream(freezer_p + "cgroup.procs") << pid;
//        write_pid(pid, fd_cpuset_procs);
//        write_pid(pid, fd_uni_procs);
//        populated = true;
//    } catch (const std::exception& e) {
//        std::cerr << "probably bad cgroup setting?" << std::endl;
//        std::cerr << e.what() << std::endl;
//        throw e;
//    }
//}

//void Cgroup::freeze()
//{
//#ifdef VERBOSE
//    std::cerr<< __PRETTY_FUNCTION__ << " " + freezer_p << std::endl;
//#endif
//    const char buf[] = "FROZEN";
//    CHECK( write(fd_freezer_state, buf, strlen(buf)) );
//}

//void Cgroup::unfreeze()
//{
//#ifdef VERBOSE
//    std::cerr<< __PRETTY_FUNCTION__ << " " + freezer_p << std::endl;
//#endif
//    const char buf[] = "THAWED";
//    CHECK( write(fd_freezer_state, buf, strlen(buf)) );
//}

//std::string Cgroup::get_name()
//{
//    return name;
//}

//int Cgroup::get_fd_cpuset_procs()
//{
//    return fd_cpuset_procs;
//}

//void Cgroup::set_cpus(std::string cpus)
//{
//    if( is_process_cgrp )
//        throw std::system_error(1, std::generic_category(), std::string(__PRETTY_FUNCTION__) + ": cannot assign cpu to process, use partition");

//    CHECK( write(fd_cpuset_cpus, cpus.c_str(), cpus.size()) );
//}

//void Cgroup::close_all_fd()
//{
//    close(fd_freezer_state);
//    close(fd_uni_procs);
//    close(fd_uni_events);

//    if( !is_process_cgrp ){
//        close(fd_cpuset_procs);
//        close(fd_cpuset_cpus);
//    }

//}


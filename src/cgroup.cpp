#include "cgroup.hpp"

void create_cgroup(std::string path)
{
    CHECK( mkdir( path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) );
}

int Cgroup::open_fd(std::string path, int attr = O_RDWR | O_NONBLOCK )
{
    return CHECK( open( path.c_str(), attr) );
}

Cgroup::Cgroup(std::string name)
    : freezer_p(freezer_path + name + "/")
    , cpuset_p(cpuset_path + name + "/")
    , unified_p(unified_path + name + "/")
{
    //std::cerr<< __PRETTY_FUNCTION__ << "@" << this << " " << freezer_p <<std::endl;
    // create new freezer cgroup
    create_cgroup( freezer_p);
    // create new cpuset cgroup
    create_cgroup( cpuset_p);
    // create unified cgroup for .events
    create_cgroup( unified_p);
    // open file descriptor for processes in freezer
    fd_freezer_procs = open_fd( freezer_p + "cgroup.procs");
    // open file descriptor for (un)freezing cgroup
    fd_freezer_state = open_fd( freezer_p + "freezer.state");
    // open file descriptor for processes in cpuset
    fd_cpuset_procs = open_fd( cpuset_p + "cgroup.procs");
    // open file descriptor for cpuset
    fd_cpuset_cpus = open_fd( cpuset_p + "cpuset.cpus");
    // open file descriptor for processes in unified cgroup
    fd_uni_procs = open_fd( unified_p + "cgroup.procs");
    // open file descriptor for montoring cleanup
    fd_uni_events = open_fd( unified_p + "cgroup.events", O_RDONLY | O_NONBLOCK);

//    ev::io procs_w;
//    procs_w.set(fd_events, ev::PRI );
//    procs_w.set<Cgroup, &Cgroup::clean_cb>(this);
//    procs_w.start();

}

void Cgroup::delete_cgroup(std::string path)
{
    // delete cgroup
    CHECK( rmdir( path.c_str()) );
}

Cgroup::~Cgroup()
{
    std::cerr<< __PRETTY_FUNCTION__ << " PID:"+std::to_string(getpid())+" @" << this << " " << freezer_p <<std::endl;
    if( fd_freezer_procs == -1 ){
        err(1,"wrong call of destructor");
    }

    this->freeze();

    // read pids from fd
    std::vector<pid_t> pids;
    FILE *f = fdopen( fd_freezer_procs, "r");
    if( f == nullptr )
        err(1,"fdopen, something wrong, need to delete cgroups manually");
    char *line = nullptr;
    size_t len = 0;
    ssize_t nread;
    //std::cerr<< ret <<" "<<errno<<" "<<line<<std::endl;
    while( (nread = getline(&line, &len, f)) != -1 ){
        pids.push_back( atoi(line) );
    }
    // check if end of file reached (getline return -1 on both error and eof)
    CHECK( !feof(f) );

    free(line);
    fclose(f);

    // empty cgroup, can be removed
    if( pids.size() == 0){
        // delete cgroups
        delete_cgroup( freezer_p );
        delete_cgroup( cpuset_p );
        delete_cgroup( unified_p );
        close_all_fd();
        return;
    }

    // kill all processes in cgroup
    for( pid_t pid : pids ){
        std::cerr<<"killing process "<<pid<<std::endl;
        CHECK( kill(pid, SIGKILL) );
    }

    this->unfreeze();
    // cgroup deleted by release_agent when empty

    //sleep(1);
//    //delete cgroup, TODO cpuset
//    if( rmdir( (freezer_path).c_str()) == -1)
//        err(1,"rmdir, need to delete cgroups manually");
    close_all_fd();
}

void Cgroup::clean_cb (ev::stat &w, int revents)
{
    std::cerr<<"tu"<<std::endl;
}

void Cgroup::write_pid(pid_t pid, int fd)
{
    // add process to cgroup (echo PID > cgroup.procs)
    char buf[20];
    int size = sprintf(buf, "%d", pid);
    CHECK(write(fd, buf, size+1));
}

void Cgroup::add_process(pid_t pid)
{
    write_pid(pid, fd_freezer_procs);
    write_pid(pid, fd_cpuset_procs);
    write_pid(pid, fd_uni_procs);
}

void Cgroup::freeze()
{
    const char buf[7] = "FROZEN";
    CHECK( write(fd_freezer_state, buf, strlen(buf)) );
}

void Cgroup::unfreeze()
{
    const char buf[] = "THAWED";
    CHECK( write(fd_freezer_state, buf, strlen(buf)) );
}

void Cgroup::close_all_fd()
{
    close(fd_freezer_procs);
    close(fd_freezer_state);
    close(fd_uni_procs);
    close(fd_uni_events);
    close(fd_cpuset_procs);
    close(fd_cpuset_cpus);
}

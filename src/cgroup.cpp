#include "cgroup.hpp"

void create_cgroup(std::string path)
{
    CHECK( mkdir( path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) );
}

int Cgroup::open_fd(std::string path, int attr = O_RDWR | O_NONBLOCK )
{
    return CHECK( open( path.c_str(), attr) );
}

Cgroup::Cgroup(ev::loop_ref loop, std::string name)
    : freezer_p(freezer_path + name + "/")
    , cpuset_p(cpuset_path + name + "/")
    , unified_p(unified_path + name + "/")
    , procs_w(loop)
{
    //std::cerr<< __PRETTY_FUNCTION__ << "@" << this << " " << freezer_p <<std::endl;

    try{
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
        fd_uni_events = open_fd( unified_p + "cgroup.events", O_RDONLY);

        procs_w.set<Cgroup, &Cgroup::clean_cb>(this);
        procs_w.start(fd_uni_events, ev::EXCEPTION);
    } catch (const std::exception& e) {
        std::cerr << "probably bad cgroup setting?" << std::endl;
        delete_cgroup();
        close_all_fd();
        throw e;
    }

    //freeze();

}

void Cgroup::delete_cgroup()
{
    // delete cgroups
    CHECK( rmdir( freezer_p.c_str()) );
    CHECK( rmdir( cpuset_p.c_str()) );
    CHECK( rmdir( unified_p.c_str()) );
    populated = false;
    deleted = true;

}

void Cgroup::kill_all()
{
    if(!populated)
        return;
    freeze();

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

    // kill all processes in cgroup
    for( pid_t pid : pids ){
        std::cerr<<"killing process "<<pid<<std::endl;
        CHECK( kill(pid, SIGKILL) );
    }

    unfreeze();
}

Cgroup::~Cgroup()
{
    std::cerr<< __PRETTY_FUNCTION__ << " PID:"+std::to_string(getpid())+" @" << this << " " << freezer_p <<std::endl;
    if(!deleted)
        delete_cgroup();
    close_all_fd();
}

void Cgroup::clean_cb (ev::io &w, int revents)
{
    std::cerr<< __PRETTY_FUNCTION__ << " PID:"+std::to_string(getpid())+" @" << this << " " << freezer_p <<std::endl;
    char buf[100];
    CHECK( pread(w.fd, buf, sizeof (buf) - 1, 0) );
    std::cerr<< buf<<std::endl;

    bool populated = (std::string(buf).find("populated 1") != std::string::npos);
    if(!populated){
        w.stop();
        delete_cgroup();
    }

//    w.stop();
//    bool populate_new = ...;
//    if (!populated && populate_new)
//       populate_cnt++;
//    else if (populated && !populate_new) {
//        poplated_cnt--;
//        if (populted_cnt == 0)
//            loop.break();
//    }
    // demos_sch.cgroup_clean(this);
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
    try{
        write_pid(pid, fd_freezer_procs);
        write_pid(pid, fd_cpuset_procs);
        write_pid(pid, fd_uni_procs);
        populated = true;
    } catch (const std::exception& e) {
        std::cerr << "probably bad cgroup setting?" << std::endl;
        std::cerr << e.what() << std::endl;
        throw e;
    }
}

void Cgroup::freeze()
{
    std::cerr<< __PRETTY_FUNCTION__ << " " + freezer_p << std::endl;
    const char buf[] = "FROZEN";
    CHECK( write(fd_freezer_state, buf, strlen(buf)) );
}

void Cgroup::unfreeze()
{
    std::cerr<< __PRETTY_FUNCTION__ << " " + freezer_p << std::endl;
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

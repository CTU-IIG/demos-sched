#include "cgroup.hpp"

Cgroup::Cgroup(std::string path)
    : freezer_path(path)
{
    // create new cgroup
    int ret = mkdir( path.c_str(),
            S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if( ret == -1 ){
        if( errno == EEXIST ) {
            throw "failed mkdir " + path + ", process name has to be unique";
        }else{
            throw "failed mkdir " + path;
        }
    }

    // open file descriptor for processes in cgroup
    fd_procs = open( (freezer_path + "cgroup.procs").c_str(), O_RDWR | O_NONBLOCK);
    if(fd_procs == -1)
        throw "failed open " + freezer_path + "cgroup.procs";

    // open file descriptor for (un)freezing cgroup
    fd_state = open( (freezer_path + "freezer.state").c_str(), O_RDWR | O_NONBLOCK);
    if(fd_state == -1)
        throw "failed open " + freezer_path + "freezer.state";

}

Cgroup::~Cgroup()
{
    std::cerr<< "destructor " + freezer_path <<std::endl;
    if( fd_procs == -1 ){
        err(1,"wrong call of destructor");
    }

    this->freeze();

    // read pids from fd
    std::vector<pid_t> pids;
    FILE *f = fdopen( fd_procs, "r");
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
    if(!feof(f))
        err(1,"getline");

    free(line);
    fclose(f);

    // empty cgroup, can be removed
    if( pids.size() == 0){
        // delete cgroup, TODO cpuset
        if( rmdir( (freezer_path).c_str()) == -1)
            err(1,"rmdir, need to delete cgroups manually");
        close(fd_procs);
        close(fd_state);
        return;
    }

    // kill all processes in cgroup
    for( pid_t pid : pids ){
        std::cerr<<"killing process "<<pid<<std::endl;
        if( kill(pid, SIGKILL) == -1){
            warn("need to kill process %d manually", pid);
        }
    }

    this->unfreeze();
    // cgroup deleted by release_agent when empty

//    sleep(1);
//    //delete cgroup, TODO cpuset
//    if( rmdir( (freezer_path).c_str()) == -1)
//        err(1,"rmdir, need to delete cgroups manually");
    close(fd_procs);
    close(fd_state);
}


void Cgroup::add_process(pid_t pid)
{
    // add process to cgroup (echo PID > cgroup.procs)
    char buf1[20];
    sprintf(buf1, "%d", pid);
    if( write(fd_procs, buf1, 20*sizeof(pid_t)) == -1)
        throw "cannot write new process to cgroup";
}

void Cgroup::freeze()
{
    char buf[7] = "FROZEN";
    if( write(fd_state, buf, 6*sizeof(char)) == -1){
        throw "cannot freeze cgroup " + freezer_path;
    }
}

void Cgroup::unfreeze()
{
    char buf[7] = "THAWED";
    if( write(fd_state, buf, 6*sizeof(char)) == -1){
        throw "cannot unfreeze cgroup " + freezer_path;
    }
}

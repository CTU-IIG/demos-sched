#include<iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include<err.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

using namespace std;

const string path = "/sys/fs/cgroup/freezer/my_freezer/new_proc/";

int main()
{
    //getpid();
    
    // create cgroup for user
    // sudo mkdir /sys/fs/cgroup/freezer/my_freezer
    // sudo chown -R <user> /sys/fs/cgroup/freezer/my_freezer
    
    // create cgroup
    int ret = mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if( ret == -1 )
        if( errno != EEXIST )
            err(1,"mkdir");
            
    int fd_procs = open( (path + "cgroup.procs").c_str(), O_RDWR | O_NONBLOCK);
    if(fd_procs == -1) err(1,"open");
    int fd_state = open( (path + "freezer.state").c_str(), O_RDWR | O_NONBLOCK);
    if(fd_state == -1) err(1,"open");

    // create new process
    pid_t pid = fork();
    if( pid == -1 )
        err(1,"fork");

    if( pid == 0 ){ // launch new process

        // add to cgroup (echo PID > cgroup.procs)
        char buf1[20];
        sprintf(buf1, "%d", getpid());
        if( write(fd_procs, buf1, 20*sizeof(pid_t)) == -1)
            err(1,"write1");
        close(fd_procs);

        // freeze (echo FROZEN > freezer.state)
        char buf2[7] = "FROZEN";
        if( write(fd_state, buf2, 6*sizeof(char)) == -1)
            err(1,"write2");
        close(fd_state);

        //if( execl("/bin/echo", "/bin/echo","hello",NULL ) == -1)
        if( execl("../build/infinite_proc", "../build/infinite_proc","100000","hello",NULL ) == -1)
            err(1,"execv");
    } else {
        // wait until process in cgroup
        sleep(1); // TODO ev callback on fd_procs, read cg.procs and check if it is there?

        // unfreeze (echo THAWED > freezer.state)
        char buf[7] = "THAWED";
        if( write(fd_state, buf, 6*sizeof(char)) == -1)
            err(1,"write3");

        // wait for a while to see something
        sleep(1);

        // kill all processes in cgroup
        //pid_t pid_to_kill;
        //while (f_procs >> pid_to_kill){
        //    kill(pid_to_kill, SIGKILL);
        //}
        kill(pid, SIGKILL);

        close(fd_procs);
        close(fd_state);
    
        // check empty cgroup (empty cgroups/procs, notify_on_release) (v2 cgroup.events (0 empty))
        // TODO

        // delete cgroup
        if( rmdir(path.c_str()) == -1)
            err(1,"rmdir");
            
    }

    return 0;
}

#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include "demossched.hpp"
#include <ev++.h>
#include <fcntl.h>

using namespace std;
using namespace ev;

void io_cb(ev::io &w, int revents)
{
    cout << "cgroup event" << endl;
    char buf[1000];

    int ret = CHECK(pread(w.fd, buf, sizeof(buf) - 1, 0));
    buf[ret] = 0;
    cout << "read " << ret << " bytes:" << endl << buf << endl;
    if (string(buf).find("populated 0") != string::npos)
        w.loop.break_loop();
}

int main(int argc, char *argv[])
{
    ifstream my_cgroups("/proc/"+to_string(getpid())+"/cgroup");
    string line;
    string cgroup;

    while (std::getline(my_cgroups, line)) {
        if (line.rfind("0::", 0) == 0) {
            cgroup = "/sys/fs/cgroup/unified" + line.substr(3) + "/asdf";
            cout << cgroup << endl;
        }
    }
    my_cgroups.close();

    rmdir(cgroup.c_str());
    CHECK(mkdir(cgroup.c_str(), 0700));

    default_loop loop;
    io cg_events(loop);

    pid_t pid = CHECK(vfork());

    if (pid == 0) {
        CHECK(execlp("sleep", "sleep", "1", 0));
    }

    ofstream((cgroup + "/cgroup.procs")) << to_string(pid);

    cg_events.set<io_cb>();
    cg_events.start(CHECK(open((cgroup + "/cgroup.events").c_str(), O_RDONLY)), ev::EXCEPTION);

    loop.run();

    CHECK(rmdir(cgroup.c_str()));

    return 0;
}

#ifndef CGROUP_HPP
#define CGROUP_HPP

// mkdir, open
#include <sys/stat.h>
#include <sys/types.h>
// open
#include <fcntl.h>
// write
#include <unistd.h>
#include <iostream>
#include <vector>
#include <err.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <ev++.h>
#include <bitset>
#include <memory>
#include <functional>
#include <list>

// maximum supported number of processors
#define MAX_NPROC 8
// cpu usage mask
typedef std::bitset<MAX_NPROC> Cpu;

class Process;

class Cgroup{
public:
    Cgroup() {};
    Cgroup(std::string path);
    Cgroup(std::string parent_path, std::string name);
    Cgroup(Cgroup& parent, std::string name /* Cgroup &garbage*/);
    ~Cgroup();

    void add_process(pid_t pid);
    //void add(const Process& proc); // ????
    void kill_all();

    // delete copy constructor
    Cgroup(const Cgroup&) = delete;
    Cgroup& operator=(const Cgroup&) = delete;

    // Move constructor/assignment
    Cgroup(Cgroup&& other) = default;
    Cgroup& operator=(Cgroup&&) = default;

    std::string getPath() const;

protected:
    //std::vector<std::unique_ptr<Cgroup>> children;
    //std::vector<Cgroup*> children;
    std::string path;
};

class CgroupFreezer : public Cgroup {
public:
    CgroupFreezer(std::string parent_path, std::string name);
    CgroupFreezer(Cgroup &parent, std::string name /* Cgroup &garbage*/);

    void freeze();
    void unfreeze();
private:
    int fd_state;
};

class CgroupCpuset : public Cgroup {
public:
    CgroupCpuset(std::string parent_path, std::string name);
    CgroupCpuset(Cgroup &parent, std::string name /* Cgroup &garbage*/);
    void set_cpus(std::string cpus);
private:
    int fd_cpus;
};

class CgroupEvents : public Cgroup {
public:
    CgroupEvents(ev::loop_ref loop, std::string parent_path, std::string name,
                 std::function<void(bool)> populated_cb);
    CgroupEvents(ev::loop_ref loop, CgroupEvents &parent, std::string name,
                 std::function<void(bool)> populated_cb);
    CgroupEvents(ev::loop_ref loop, Cgroup &parent, std::string name,
                 std::function<void(bool)> populated_cb);
    ~CgroupEvents();
private:
    int fd_events;
    ev::io events_w;
    std::function<void(bool)> populated_cb;
    void clean_cb(ev::io &w, int revents);
};

//void cb(int i) {

//}


//Cgroup parent("a", "b");
//CgroupEvents cge(ev::default_loop(), parent, "xxx");

//class CgroupXXX
//{
//public:
//    Cgroup(ev::loop_ref loop, bool process_cgrp, std::string cgrp_name = "",
//           int fd_cpuset_procs = -1, std::string partition_cgroup_name = "");
//    ~Cgroup();
//    void add_process(pid_t pid);
//    void freeze();
//    void unfreeze();

//    std::string get_name();
//    int get_fd_cpuset_procs();

//    void set_cpus(std::string cpus);

//    // delete copy constructor
//    Cgroup(const Cgroup&) = delete;
//    Cgroup& operator=(const Cgroup&) = delete;

//    Cgroup(Cgroup&& src);

//    void kill_all();
//private:
//    std::vector<std::unique_ptr<Cgroup>> children;
//    static int cgrp_count;
//    std::string name;
//    //int fd_freezer_procs;
//    int fd_freezer_state;
//    int fd_uni_procs;
//    int fd_uni_events;
//    int fd_cpuset_procs = -1;
//    int fd_cpuset_cpus = -1;
//    const std::string freezer_p;
//    const std::string cpuset_p;
//    const std::string unified_p;
//    ev::io procs_w;
//    bool populated = false;
//    bool killed = false;
//    bool deleted = false;
//    bool is_process_cgrp;

//    ev::loop_ref loop;

//    void clean_cb(ev::io &w, int revents);
//    void close_all_fd();
//    void delete_cgroup();
//    void write_pid(pid_t pid, int fd);
//    int open_fd(std::string path, int attr);
//};

#endif // CGROUP_HPP

##compilation
    ```
    meson build
    cd build
    ninja
    ```

## settings
- move shell for demos-sched execution to user delegated v2 cgroup (man cgroups, v2 delegation)
    ```
    mkdir /sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_shell
    sudo echo $$ > .../my_shell/cgroup.procs
    ```
- set clone_children flag in cpuset
    ```
    sudo echo 1 > .../cpuset/cgroup.clone_children
    ```
- create freezer and cpuset v1 cgroup and delegate it to the user
    ```
    mkdir /sys/fs/cgroup/freezer/my_cgroup
    sudo chown -R <user> ...
    /sys/fs/cgroup/cpuset/my_cgroup
    sudo chown -R <user> ...
    ```


- set notify_on_release
    ```
    echo 1 > .../my_cgroup/notify_on_release
    ```
- set release_agent script
    ```
    sudo echo "<demos-sched folder>/src/release_agent.sh" > /sys/fs/cgroup/freezer/release_agent
    ```

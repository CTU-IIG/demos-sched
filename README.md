## compilation

It is recommended, though not necessary, to uses Meson >= 0.52. If
your distribution has an older version, you can install a new one by:

    pip3 install --user meson

Then build everything by running:

    make

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
- create v2 cgroup
    ```
    mkdir /sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup
    ```


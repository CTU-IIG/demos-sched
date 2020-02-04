##compilation
    ```
    meson build
    cd build
    ninja
    ```

## settings

- create freezer and cpuset cgroup and delegate it to the user
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

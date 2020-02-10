#!/bin/bash

mkdir /sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_shell
mkdir /sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup
#sudo echo $$ > .../my_shell/cgroup.procs

su

mkdir /sys/fs/cgroup/freezer/my_cgroup
chown -R zahorji2 /sys/fs/cgroup/freezer/my_cgroup

echo 1 > /sys/fs/cgroup/cpuset/cgroup.clone_children
mkdir /sys/fs/cgroup/cpuset/my_cgroup
chown -R zahorji2 /sys/fs/cgroup/cpuset/my_cgroup

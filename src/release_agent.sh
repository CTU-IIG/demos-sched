#!/bin/bash

rmdir `ls -d /sys/fs/cgroup/freezer/my_cgroup/*/*/`
rmdir `ls -d /sys/fs/cgroup/freezer/my_cgroup/*/`
rmdir `ls -d /sys/fs/cgroup/cpuset/my_cgroup/*/*/`
rmdir `ls -d /sys/fs/cgroup/cpuset/my_cgroup/*/`
rmdir `ls -d /sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup/*/*/`
rmdir `ls -d /sys/fs/cgroup/unified/user.slice/user-1000.slice/user@1000.service/my_cgroup/*/`

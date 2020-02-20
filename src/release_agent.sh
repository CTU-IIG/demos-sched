#!/bin/bash

#if [ -z "$1" ] then
#    CG_NAME="demos"
#else
#    CG_NAME="$1"
#fi

FREEZER="/sys/fs/cgroup/freezer/demos"
CPUSET="/sys/fs/cgroup/cpuset/demos"
UNIFIED="/sys/fs/cgroup/unified/user.slice/user-1000.slice/demos"

# kill all procs in freezer
procs_files=($(find "$FREEZER" -name *.procs))

for i in "${procs_files[@]}"
do
    while read line
    do
        if [ $line != $$ ]
        then
            kill $line
        fi
    done < "$i"
done

# unfreeze all freezers
freezers=($(find "$FREEZER" -name *freezer.state))

for i in "${freezers[@]}"
do
    #echo "$i"
    echo "THAWED" > "$i"
done

# remove all cgroups
d="/*"
rmdir `ls -d $FREEZER$d$d/ 2>/dev/null ` 2>/dev/null
rmdir `ls -d $FREEZER$d/ 2>/dev/null ` 2>/dev/null
rmdir `ls -d $CPUSET$d$d/ 2>/dev/null ` 2>/dev/null
rmdir `ls -d $CPUSET$d/ 2>/dev/null ` 2>/dev/null
rmdir `ls -d $UNIFIED$d$d/ 2>/dev/null ` 2>/dev/null
rmdir `ls -d $UNIFIED$d/ 2>/dev/null ` 2>/dev/null

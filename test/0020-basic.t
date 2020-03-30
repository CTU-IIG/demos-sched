#!/bin/bash
. testlib
plan_tests 4

out=$(demos-sched -C "bad config")
is $? 1 "bad config exit code"
okx demos-sched -C "{windows: [], partitions: []}"

out=$(demos-sched -C "{
    windows: [ {length: 50, slices: [ { cpu: 0, sc_partition: SC1 }] } ],
    partitions: [ { name: SC1, processes: [ { cmd: echo hello, budget: 500 } ] } ]
}")
is $? 0 "exit code"
is "$out" "hello" "hello is printed"

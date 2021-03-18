#!/usr/bin/env bash
. testlib
plan_tests 5

out=$(demos-sched -C "bad config")
is $? 1 "bad config => exit code 1"
demos-sched -C "{windows: [], partitions: []}"
is $? 1 "empty config => exit code 1"

out=$(demos-sched -C "{
    windows: [ {length: 50, slices: [ { cpu: 0, sc_partition: SC1 }] } ],
    partitions: [ { name: SC1, processes: [ { cmd: echo hello, budget: 500 } ] } ]
}")
is "$out" "hello" "hello is printed"

okx demos-sched -C '{windows: [{length: 100, sc_partition: [ true ]}]}'

# Tests that demos correctly prints synchronization message on each window start
out=$(demos-sched -m "<SYNC>" -C '
windows:
  - length: 100
    slices: [{cpu: 0, sc_partition: SC1}]
  - length: 10
    slices: [{cpu: 0, sc_partition: SC1}]
  - length: 15
    slices: [{cpu: 0, sc_partition: SC1}]

partitions:
  - name: SC1
    processes: [{budget: 1000, cmd: api-test -1 -2}]
')
is "$out" '<SYNC>
-1
<SYNC>
-2
<SYNC>' \
"Synchronization messages are correctly printed"
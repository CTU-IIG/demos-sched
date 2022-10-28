#!/usr/bin/env bats
load testlib

@test "bad config" {
    run -1 demos-sched -C "bad config"
}

@test "empty config" {
    run -1 demos-sched -C "{windows: [], partitions: []}"
}

@test "executing echo hello (canonical config)" {
    run -0 demos-sched -C "{
        windows: [ {length: 50, slices: [ { cpu: 0, sc_partition: SC1 }] } ],
        partitions: [ { name: SC1, processes: [ { cmd: echo hello, budget: 500 } ] } ]
    }"
    [[ $output =~ "hello" ]]
}

@test "executing true (simplified config)" {
    run -0 demos-sched -C '{windows: [{length: 100, sc_partition: [ true ]}]}'
}

@test "correct printing of synchronization messages" {
    run -0 --separate-stderr demos-sched -m "<WINDOW>" -M "<MF>" -C '
windows:
  - length: 60
    slices: [{cpu: 0, sc_partition: SC1}]
  - length: 10
    slices: [{cpu: 0, sc_partition: SC1}]
  - length: 10
    slices: [{cpu: 0, sc_partition: SC1}]

partitions:
  - name: SC1
    processes: [{budget: 1000, cmd: api-test 1 2 3}]
'
    [[ $output = '<MF>
<WINDOW>
1
<WINDOW>
2
<WINDOW>
3
<MF>
<WINDOW>' ]]
}

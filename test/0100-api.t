#!/usr/bin/env bats
load testlib

@test "running api-test outside of demos-sched fails" {
    run ! api-test
}

@test "demos schedules first two iterations of the sub-process" {
    run -0 --separate-stderr demos-sched -C "{
    windows: [ {length: 50, slices: [ { cpu: 0, sc_partition: SC1 }] } ],
    partitions: [ { name: SC1, processes: [ { cmd: api-test first second, budget: 500 } ] } ]
}"
    [[ ${lines[0]} = "first" ]]
    [[ ${lines[1]} = "second" ]]
}

@test "demos doesn't hang when process exits during initialization" {
# 3 seconds should be long enough, but fundamentally, it's a race condition
    run -0 timeout 3s demos-sched -C '
windows:
  - length: 1
    slices:
      - cpu: 0
        sc_partition: SC1

partitions:
  - name: SC1
    processes:
      - cmd: exit
        budget: 100
        init: yes
      - cmd: exit
        budget: 100
'
}

@test "each process has opportunity to initialize before window scheduler starts" {
# this assumes that your shell (/bin/sh) starts up in under ~40ms
# alternative would be to make the window longer (and adjust sleep in api-init-test),
#  but that would make the test itself longer
    run -0 --separate-stderr demos-sched -m "<test>" -C '
windows:
  - length: 50
    slices:
      - {cpu: 0, sc_partition: SC1}
      - {cpu: 1, sc_partition: SC2}

partitions:
  - name: SC1
    processes: [{cmd: api-init-test, budget: 50, init: yes}]
  - name: SC2
    processes: [{cmd: api-init-test, budget: 50, init: yes}]
'
    [[ $output = \
"init start
init start
init done
init done
<test>" ]]
}


@test "calling 'demos-signal-completion' ends initialization" {
    run -0 --separate-stderr timeout 3s demos-sched -m "<win>" -C '
windows:
  - length: 100
    slices:
      - {cpu: 0, sc_processes: {init: yes,
            cmd: "echo init && demos-signal-completion && echo done"}}
'
    [[ $output = \
"init
<win>
done" ]]
}

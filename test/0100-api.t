#!/usr/bin/env bash
. testlib
plan_tests 5

api-test
is $? 1 "Running outside of demos-sched fails"

# Check that demos schedules first two iterations of the sub-process
readarray -t lines < <(demos-sched -C "{
    windows: [ {length: 50, slices: [ { cpu: 0, sc_partition: SC1 }] } ],
    partitions: [ { name: SC1, processes: [ { cmd: api-test first second, budget: 500 } ] } ]
}")
echo PATH=$PATH >&2
is "${lines[0]}" "first" "first iteration"
is "${lines[1]}" "second" "second iteration"


# Check that demos doesn't hang when process exits during initialization
# 3 seconds should be long enough, but fundamentally, it's a race condition
timeout 3s demos-sched -C '
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
ok $? "Does not hang on process exit during initialization"


# Check that each process has opportunity to initialize before window scheduler starts
out=$(demos-sched -m "<test>" -C '
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
')
is "$out" \
"init start
init done
init start
init done
<test>"
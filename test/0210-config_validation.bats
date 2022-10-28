#!/usr/bin/env bats
load testlib

# set fixed log level for config tests; otherwise if user would run something
#  like `SPDLOG_LEVEL=trace make test`, the output would include the logs and the tests would fail
export SPDLOG_LEVEL=warning

test_validation_pass() {
    local cfg_in=$1

    run -0 demos-sched -d -C "$cfg_in"
}

test_validation_fail() {
    local cfg_in=$1
    local error_like_expected=$2

    local exit_code
    run -1 demos-sched -d -C "$cfg_in" 2>&1
    echo "$output"
    [[ $output =~ "$error_like_expected" ]]
}

@test "unknown partition fails" {
    test_validation_fail "{
    windows: [{length: 1, cpu: 0, sc_partition: NON_EXISTENT}],
    partitions: [],
}" "unknown partition 'NON_EXISTENT'"
}

@test "overlapping CPU sets inside window fails" {
    test_validation_fail "{
    windows: [{length: 1, slices: [
        {cpu: 0, sc_partition: SC1},
        {cpu: 0, sc_partition: SC2},
    ]}],
    partitions: [
        {name: SC1, processes: {cmd: echo, budget: 1}},
        {name: SC2, processes: {cmd: echo, budget: 1}},
    ],
}" "overlapping CPU sets"
}

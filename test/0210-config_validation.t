#!/usr/bin/env bash
. testlib
plan_tests 4

# set fixed log level for config tests; otherwise if user would run something
#  like `SPDLOG_LEVEL=trace make test`, the output would include the logs and the tests would fail
export SPDLOG_LEVEL=warning

test_validation_pass() {
    local test_name=$1
    local cfg_in=$2

    echo "======== NEXT TEST =======================" >&2
    demos-sched -d -C "$cfg_in" >&2
    is $? 0 "$1"
}

test_validation_fail() {
    local test_name=$1
    local cfg_in=$2
    local error_like_expected=$3

    local exit_code
    local demos_output # `local` must be separate, otherwise it overwrites the exit code from DEmOS
    echo "======== NEXT TEST =======================" >&2
    demos_output=$(demos-sched -d -C "$cfg_in" 2>&1)
    exit_code=$?
    echo "$demos_output" >&2
    is $exit_code 1 "$1 - DEmOS should exit with code 1"
    like "$demos_output" "$error_like_expected" "$test_name - error output matches"
}

test_validation_fail "unknown partition" "{
    windows: [{length: 1, cpu: 0, sc_partition: NON_EXISTENT}],
    partitions: [],
}" "unknown partition 'NON_EXISTENT'"

test_validation_fail "overlapping CPU sets inside window" "{
    windows: [{length: 1, slices: [
        {cpu: 0, sc_partition: SC1},
        {cpu: 0, sc_partition: SC2},
    ]}],
    partitions: [
        {name: SC1, processes: {cmd: echo, budget: 1}},
        {name: SC2, processes: {cmd: echo, budget: 1}},
    ],
}" "overlapping CPU sets"

#!/usr/bin/env bash
. testlib
plan_tests 15

# Test that config files that we ship in test_config can be at least
# parsed. Several of them are hardware dependent and cannot be
# executed on arbitrary hardware. Therefore, their automated testing
# is problematic.

# set fixed log level for config tests; otherwise if user would run something
#  like `SPDLOG_LEVEL=trace make test`, the output would include the logs and the tests would fail
export SPDLOG_LEVEL=warning

test_config() {
    local cfg_in=$1

    okx demos-sched -d -c "../test_config/$cfg_in"
}

test_config CPU_switching.yaml
test_config api_init_test.yaml
test_config api_test-multiple_slices.yaml
test_config api_test.yaml
test_config be_partition_window_overflow.yaml
test_config be_wait.yaml
test_config environment_test.yaml
test_config init_slice_cpuset_loading.yaml
test_config jitter_process.yaml
test_config more_partitions.yaml
test_config multiple_processes.yaml
test_config per_process_power_policy.yaml
test_config per_process_power_policy2.yaml
test_config per_process_power_policy_tx2.yaml
test_config per_slice_power_policy.yaml

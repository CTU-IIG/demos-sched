#!/usr/bin/env bats

load testlib

@test "-h prints help" {
    run -0 demos-sched -h
    [[ ${lines[0]} =~ "Usage: demos-sched" ]]
}

#!/bin/bash
. testlib
plan_tests 1

api-test
is $? 1 "Running outside of demos-sched fails"

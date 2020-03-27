#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import time
from datetime import timedelta
from core.test_run import TestRun
from utils.iotrace import IotracePlugin


def test_remove_trace():
    num_iterations = 4
    TestRun.LOGGER.info("Testing removing traces")

    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    with TestRun.step("Remove existing traces"):
        if iotrace.get_latest_trace_path(prefix="'*'") != "":
            iotrace.remove_traces(prefix="'*'", force=True)
    with TestRun.step("Gather a list of valid and invalid traces"):
        for _ in range(num_iterations):
            seconds = TestRun.executor.run_expect_success("date +%S").stdout
            # If we're close to a next minute (so the pair of traces may not
            # share minutes), sleep
            if int(seconds) > 50:
                time.sleep(10)
            iotrace.start_tracing(timeout=timedelta(seconds=10))
            time.sleep(1)
            iotrace.stop_tracing()
            iotrace.start_tracing(timeout=timedelta(seconds=10))
            time.sleep(1)
            iotrace.kill_tracing()
            seconds = TestRun.executor.run_expect_success("date +%S").stdout
            # Sleep until next minute rolls around
            time.sleep(65 - int(seconds))

    with TestRun.step("Remove first trace using common prefix"):
        full_trace_list = iotrace.get_traces_list()[0]['trace']

        trace_path = full_trace_list[0]['tracePath']
        iotrace.remove_traces(prefix=trace_path[:-2]+"*")
        count = iotrace.get_trace_count()

        if count != len(full_trace_list)-1:
            TestRun.fail(f"Only one trace should be removed. Expected "
                         f"{len(full_trace_list) - 1}, "
                         f"got {count}")

    with TestRun.step("Remove second trace using common prefix"):
        iotrace.remove_traces(prefix=trace_path[:-2]+"*", force=True)
        count = iotrace.get_trace_count()
        if count != len(full_trace_list)-2:
            TestRun.fail(f"Two traces should be removed. Expected "
                         f"{len(full_trace_list) - 2}, "
                         f"got {count}")

    with TestRun.step("Remove all traces"):
        iotrace.remove_traces(prefix="'*'", force=True)
        count = iotrace.get_trace_count()
        if count != 0:
            TestRun.fail(f"All traces should be removed. Expected 0, "
                         f"got {count}")

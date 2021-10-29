#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

import time
from datetime import timedelta

from core.test_run import TestRun


def test_iotrace_list():
    """
        title: Test iotrace -M -L functionality
        description: |
          Run 3 traces with set runtime and known label to obtain "COMPLETE" traces. Then, run
          trace which will be running in-background at time of test. Get trace list and verify its
          contents.
        pass_criteria:
          - number of trace list entries matches expected
          - labels of traces match expected
          - state of completed traces is "COMPLETE" and for ongoing trace is "RUNNING"
          - filtering the list by using prefix returns apropriate number of trace entries
    """

    disk = TestRun.dut.disks[0]
    iotrace = TestRun.plugins["iotrace"]

    expected_labels = ["first", "second", "third", "fourth"]

    with TestRun.step("Populate list with some traces"):
        for trace_label in expected_labels[:-1]:
            iotrace.start_tracing(
                [disk.system_path], timeout=timedelta(seconds=5), label=trace_label
            )
            time.sleep(8)

    with TestRun.step("Run in-progress trace"):
        iotrace.start_tracing([disk.system_path], label=expected_labels[-1])
        time.sleep(4)

    with TestRun.step("Check iotrace list trace count"):
        trace_list = iotrace.get_traces_list()
        exp_len, act_len = len(expected_labels), len(trace_list)

        if exp_len != act_len:
            TestRun.LOGGER.error(
                f"Wrong number of trace entries in list. Expected: {exp_len}, got: {act_len}"
            )

    with TestRun.step("Check trace labels and statuses"):
        for trace, exp_label in zip(iotrace.get_traces_list(), expected_labels):
            if trace["label"] != exp_label:
                TestRun.LOGGER.error(
                    f"Wrong trace label. Expected: {exp_label}, got: {trace['label']}"
                )

            exp_state = "COMPLETE" if exp_label in expected_labels[:-1] else "RUNNING"
            if trace["state"] != exp_state:
                TestRun.LOGGER.error(
                    f"Wrong trace state. Expected: {exp_state}, got: {trace['state']}"
                )

    with TestRun.step("Check listing by prefix"):
        trace_list = iotrace.get_traces_list(prefix="kernel*")

        exp_len, act_len = len(expected_labels), len(trace_list)

        if exp_len != act_len:
            TestRun.LOGGER.error(
                f"Wrong number of trace entries in list. Expected: {exp_len}, got: {act_len}"
            )

        last_trace_prefix = trace_list[-1]["tracePath"]
        trace_list = iotrace.get_traces_list(prefix=last_trace_prefix)

        act_len = len(trace_list)

        if act_len != 1:
            TestRun.LOGGER.error(
                f"Single trace entry should be returned if using exact path as prefix. "
                "Actual length of list: {act_len}"
            )

        trace_list = iotrace.get_traces_list(prefix="velociraptor*")

        act_len = len(trace_list)

        if act_len != 0:
            TestRun.LOGGER.error(
                f"There should be no traces matching 'velociraptor*' prefix. "
                "Actual length of list: {act_len}"
            )

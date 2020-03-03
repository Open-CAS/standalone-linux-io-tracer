#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from datetime import timedelta
from time import sleep

from core.test_run import TestRun
from utils.iotrace import IotracePlugin, parse_json

runtime = timedelta(seconds=8)
label = "super_iotracer_test_instance"


def test_compare_summary():
    """
        title: Compare trace summaries.
        description: |
          Verify the summary returned at the end of tracing
          (output of 'iotrace –S') is the same as the summary read
          from trace files (output of 'iotrace -M -G').
        pass_criteria:
          - Trace summaries are the same.
    """
    with TestRun.step("Run tracing for a while"):
        disk = TestRun.dut.disks[0]
        output = parse_json(TestRun.executor.run(
            f'iotrace -S -d {disk.system_path} -t {int(runtime.total_seconds())}').stdout)[-1]

    with TestRun.step("Compare tracing summaries."):
        summary = IotracePlugin.get_trace_summary(output["tracePath"])

        if output != summary:
            err_output = "The summary read from trace file differs from summary " \
                         "returned at the end of tracing.\n"
            fault = False

            if summary["tracePath"] != output["tracePath"]:
                fault = True
                err_output += (f'Trace paths are different:'
                               f'\ntrace path from file: "{summary["tracePath"]}"'
                               f'\ntrace path from output: "{output["tracePath"]}"\n')
            if summary["state"] != output["state"]:
                fault = True
                err_output += (f'Trace states are different:'
                               f'\ntrace state from file: "{summary["state"]}"'
                               f'\ntrace state from output: "{output["state"]}"\n')
            if summary["sourceNode"]["node"][0]["id"] != output["sourceNode"]["node"][0]["id"]:
                fault = True
                err_output += (f'Node\'s IDs are different:'
                               f'\nnode\'s ID from file: {summary["sourceNode"]["node"][0]["id"]}'
                               f'\nnode\'s ID from output: '
                               f'{output["sourceNode"]["node"][0]["id"]}\n')
            if summary["traceStartDateTime"] != output["traceStartDateTime"]:
                fault = True
                err_output += (f'Trace start times are different:'
                               f'\ntrace start time from file: {summary["traceStartDateTime"]}'
                               f'\ntrace start time from output: {output["traceStartDateTime"]}\n')
            if summary["traceDuration"] != output["traceDuration"]:
                fault = True
                err_output += (f'Trace duration times are different:'
                               f'\ntrace duration time from file: {summary["traceDuration"]}'
                               f'\ntrace duration time from output: {output["traceDuration"]}\n')
            if summary["label"] != output["label"]:
                fault = True
                err_output += (f'Trace labels are different:'
                               f'\ntrace label from output: "{summary["label"]}"'
                               f'\nexpected trace label: "{output["label"]}"\n')
            if fault:
                TestRun.fail(err_output)

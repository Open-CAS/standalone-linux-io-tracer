#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from core.test_run import TestRun
from utils.iotrace import IotracePlugin

iterations = 50


def test_stress_iotracer():
    """
        title: Stress test for starting and stopping io-tracer.
        description: |
          Validate the ability of io-tracer to start and stop
          quickly in the loop.
        pass_criteria:
          - No system crash.
          - Io-tracer can handle many quick starts and stops.
    """
    with TestRun.step("Prepare io-tracer."):
        iotrace: IotracePlugin = TestRun.plugins["iotrace"]

    for _ in TestRun.iteration(
            range(0, iterations), f"Start and stop CAS {iterations} times."):
        with TestRun.step("Start io-tracer."):
            iotrace.start_tracing()

        with TestRun.step("Stop io-tracer."):
            iotrace.stop_tracing()

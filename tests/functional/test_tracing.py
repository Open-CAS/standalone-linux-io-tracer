#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest

from core.test_run import TestRun
from utils.iotrace import IotracePlugin


def test_iotracer_multiple_instances():
    """
        title: Check if it’s possible to run concurrent tracing processes.
        description: |
          Try to run multiple concurrent io-tracers for the same disks.
        pass_criteria:
          - No system crash.
          - It’s impossible to run io-tracer multiple times for the same disks.
    """
    with TestRun.step("Run the first io-tracer instance"):
        iotracer1 = IotracePlugin(TestRun.plugins['iotrace'].repo_dir,
                                  TestRun.plugins['iotrace'].working_dir)
        iotracer1.start_tracing()

    with TestRun.step("Check if the first instance of io-tracer works."):
        if not iotracer1.check_if_tracing_active():
            TestRun.fail("There is no working io-tracer instance.")
        TestRun.LOGGER.info(f"Iotrace process found with PID {iotracer1.pid}")

    with TestRun.step("Try to start the second iotracer instance."):
        iotracer2 = IotracePlugin(TestRun.plugins['iotrace'].repo_dir,
                                  TestRun.plugins['iotrace'].working_dir)
        iotracer2.start_tracing()
        if iotracer2.check_if_tracing_active():
            TestRun.fail("There is working iotracer second instance.")
        TestRun.LOGGER.info(
            "Cannot start the second instance of iotracer as expected.")

    with TestRun.step("Stop io-tracer."):
        iotracer1.stop_tracing()

#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from time import sleep
import pytest

from core.test_run import TestRun
from utils.iotrace import IotracePlugin
from storage_devices.disk import DiskType, DiskTypeLowerThan, DiskTypeSet


def test_iotracer_multiple_instances_same_disks():
    """
        title: Check if it’s possible to run concurrent tracing processes.
        description: |
          Try to run concurrent io-tracers for the same disks.
        pass_criteria:
          - No system crash.
          - It’s impossible to run io-tracer multiple times for the same disks.
    """
    with TestRun.step("Run the first io-tracer instance"):
        iotracer1 = IotracePlugin()
        iotracer1.start_tracing()

    with TestRun.step("Check if the first instance of io-tracer works."):
        if not iotracer1.check_if_tracing_active():
            TestRun.LOGGER.error("There is no working io-tracer instance.")
        TestRun.LOGGER.info(f"Iotrace process found with PID {iotracer1.pid}")

    with TestRun.step("Try to start the second iotracer instance."):
        iotracer2 = IotracePlugin()
        iotracer2.start_tracing()
        if iotracer2.check_if_tracing_active():
            TestRun.LOGGER.error("There is working iotracer second instance.")
        TestRun.LOGGER.info(
            "Cannot start the second instance of iotracer as expected.")

    with TestRun.step("Stop io-tracer."):
        iotracer1.stop_tracing()


def test_iotracer_multiple_instances_other_disks():
    """
        title: Check if it’s possible to run concurrent tracing processes.
        description: |
          Try to run concurrent io-tracers for different disks.
        pass_criteria:
          - No system crash.
          - It’s impossible to run io-tracer concurrently for many disks.
    """
    with TestRun.step("Run the first io-tracer instance"):
        iotracer1 = IotracePlugin()
        iotracer1.start_tracing([TestRun.dut.disks[0].system_path])
        sleep(1)

    with TestRun.step("Check if the first instance of io-tracer works."):
        if not iotracer1.check_if_tracing_active():
            TestRun.LOGGER.error("There is no working io-tracer instance.")
        TestRun.LOGGER.info(f"Iotrace process found with PID {iotracer1.pid}")

    with TestRun.step("Try to start the second iotracer instance."):
        iotracer2 = IotracePlugin()
        iotracer2.start_tracing([TestRun.dut.disks[1].system_path])
        sleep(1)
        if iotracer2.check_if_tracing_active():
            TestRun.LOGGER.error("There is working iotracer second instance.")
        TestRun.LOGGER.info("Cannot start the second instance of iotracer as expected.")

    with TestRun.step("Stop io-tracer."):
        iotracer1.stop_tracing()

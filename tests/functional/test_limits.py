#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from datetime import timedelta
from math import isclose
from time import sleep
import pytest

from core.test_run import TestRun
from test_tools.fio.fio import Fio
from test_tools.fio.fio_param import IoEngine, ReadWrite
from test_utils.size import Size, Unit
from utils.iotrace import IotracePlugin

"""
If you're running this test on HDD, change size limits to few and few dozens MiB,
otherwise you'll be waiting for completion very long time. Also raise test timeout - for tracings 
based on trace file size limit.
"""
runtime_short = timedelta(seconds=20)
size_limit_low = Size(1, Unit(Unit.GibiByte))
runtime_long = timedelta(hours=2)
size_limit_high = Size(20, Unit(Unit.GibiByte))

test_timeout = int(timedelta(minutes=30).total_seconds())
fio_runtime = timedelta(hours=48)


def test_iotracer_limits():
    """
        title: Check if io-tracer respects its own tracing limits .
        description: |
          Check if io-tracer respects the time limit and the trace file size limit
          set with start command and finishes tracing when one of two limits is reached.
        pass_criteria:
          - No system crash.
          - Tracing stops when one of the limits is reached.
    """
    with TestRun.step("Generate workload on device."):
        disk = TestRun.dut.disks[0]
        fio_pid = fio_workload(disk.system_path, fio_runtime).run_in_background()

    with TestRun.step("Prepare io-tracer."):
        iotracer: IotracePlugin = TestRun.plugins['iotrace']

    with TestRun.step("Run io-tracer with duration limit set."):
        iotracer.start_tracing([disk.system_path], timeout=runtime_short)

    with TestRun.step("Wait for tracing to finish."):
        wait(iotracer)
        output = IotracePlugin.get_trace_summary(IotracePlugin.get_latest_trace_path())

    with TestRun.step("Check tracing duration."):
        if not is_time_almost_equal(runtime_short, output['traceDuration']):
            TestRun.LOGGER.error("Tracing duration is different than set.")

    with TestRun.step("Run io-tracer with file size limit set."):
        iotracer.start_tracing([disk.system_path], trace_file_size=size_limit_low)

    with TestRun.step("Wait for tracing to finish."):
        wait(iotracer)
        output = IotracePlugin.get_trace_summary(IotracePlugin.get_latest_trace_path())

    with TestRun.step("Check trace file size."):
        if not is_size_almost_equal(size_limit_low, output['traceSize']):
            TestRun.LOGGER.error("Tracing file size is different than set.")

    with TestRun.step("Run io-tracer with low file size limit and high duration limit set."):
        iotracer.start_tracing([disk.system_path], timeout=runtime_long,
                               trace_file_size=size_limit_low)

    with TestRun.step("Wait for tracing to finish."):
        wait(iotracer)
        output = IotracePlugin.get_trace_summary(IotracePlugin.get_latest_trace_path())

    with TestRun.step("Check tracing duration and trace file size."):
        if not ((is_time_almost_equal(runtime_long, output['traceDuration'])
                 and is_size_lower_or_equal(output['traceSize'], size_limit_low))
                or (is_time_lower_or_equal(output['traceDuration'], runtime_long)
                    and is_size_almost_equal(size_limit_low, output['traceSize']))):
            TestRun.LOGGER.error("Tracing did not stop after reaching size nor time limit.")

    with TestRun.step("Run io-tracer with high file size limit and low duration limit set."):
        iotracer.start_tracing([disk.system_path], timeout=runtime_short,
                               trace_file_size=size_limit_high)

    with TestRun.step("Wait for tracing to finish."):
        wait(iotracer)
        output = IotracePlugin.get_trace_summary(IotracePlugin.get_latest_trace_path())

    with TestRun.step("Check tracing duration and trace file size."):
        if not ((is_time_almost_equal(runtime_short, output['traceDuration'])
                 and is_size_lower_or_equal(output['traceSize'], size_limit_high))
                or (is_time_lower_or_equal(output['traceDuration'], runtime_short)
                    and is_size_almost_equal(size_limit_high, output['traceSize']))):
            TestRun.LOGGER.error("Tracing did not stop after reaching size nor time limit.")

    with TestRun.step("Stop fio workload."):
        TestRun.executor.kill_process(fio_pid)


def is_size_almost_equal(size_a: Size, size_b: str):
    """Returns true if both sizes are equal +/- 10%"""
    return isclose(int(size_a.get_value(Unit.MebiByte)), int(size_b), rel_tol=0.1)


def is_time_almost_equal(time_a: timedelta, time_b: str):
    """Returns true if both times are equal +/- 5s"""
    return isclose(int(time_a.total_seconds()), int(time_b), abs_tol=5)


def is_size_lower_or_equal(size_a: str, size_b: Size):
    """Returns true if size_a is lower or equal than size_b"""
    return int(size_a) <= int(size_b.get_value(Unit.MebiByte))


def is_time_lower_or_equal(time_a: str, time_b: timedelta):
    """Returns true if time_a is lower or equal than time_b"""
    return int(time_a) <= int(time_b.total_seconds())


def fio_workload(target: str, runtime: timedelta):
    fio_run = Fio().create_command()
    fio_run.io_engine(IoEngine.libaio)
    fio_run.direct()
    fio_run.time_based()
    fio_run.run_time(runtime)
    fio_run.io_depth(64)
    fio_run.read_write(ReadWrite.readwrite)
    fio_run.target(target)
    fio_run.block_size(int(Size(4, Unit.KibiByte)))

    return fio_run


def wait(iotracer: IotracePlugin):
    time = 0
    while iotracer.check_if_tracing_active():
        sleep(2)
        time += 2
        if time >= test_timeout:
            iotracer.stop_tracing()
            TestRun.fail("Tracing reached test time limit.")

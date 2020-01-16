#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import datetime

from core.test_run import TestRun
from iotrace import IotracePlugin
from utils.fio import run_workload
from test_tools.fio.fio_param import ReadWrite


def test_data_performance_120s():
    TestRun.LOGGER.info("Testing performance drop with tracing")
    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    runtime = datetime.timedelta(seconds=120)
    jobs = 16
    method = ReadWrite.randread

    for disk in TestRun.dut.disks:
        with TestRun.step("Run test random read workload without tracing"):
            results = run_workload(disk.system_path, runtime, verify=False, num_jobs=jobs, method=method)
            clean_iops = sum(job.read_iops() for job in results)
            TestRun.LOGGER.info("Recorded " + str(clean_iops) + " IOPS without tracing")
        with TestRun.step("Start tracing"):
            iotrace.start_tracing([disk.system_path])
        with TestRun.step("Run test random read workload with tracing"):
            trace = run_workload(disk.system_path, runtime, verify=False, num_jobs=jobs, method=method)
            trace_iops = sum(job.read_iops() for job in trace)
            TestRun.LOGGER.info("Recorded " + str(trace_iops) + " IOPS with tracing")
        with TestRun.step("Stop tracing"):
            iotrace.stop_tracing()

        if trace_iops / clean_iops < 0.95:
            raise Exception("Excessive performance drop during tracing")

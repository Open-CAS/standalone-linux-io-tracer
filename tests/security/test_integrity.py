#
# Copyright(c) 2019-2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

import datetime
import time
import pytest

from core.test_run import TestRun
from utils.iotrace import IotracePlugin
from utils.fio import run_workload, setup_workload

seconds = 20
runtime = datetime.timedelta(seconds=seconds)


def test_data_integrity_20s():
    TestRun.LOGGER.info("Testing data integrity")
    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    for disk in TestRun.dut.disks:
        with TestRun.step("Start tracing"):
            iotrace.start_tracing([disk.system_path])
        with TestRun.step("Run test workloads with verification"):
            run_workload(disk.system_path, runtime)
        with TestRun.step("Stop tracing"):
            iotrace.stop_tracing()


def test_data_integrity_restart_tracing():
    TestRun.LOGGER.info("Testing data integrity with multiple trace starts")
    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    for disk in TestRun.dut.disks:
        with TestRun.step("Run test workloads with verification in background"):
            fio_run = setup_workload(disk.system_path, runtime)
            pid = fio_run.run_in_background()
        with TestRun.step("Start and stop tracing multiple times"):
            for i in range(seconds):
                iotrace.start_tracing([disk.system_path])
                iotrace.stop_tracing()
                time.sleep(1)
        with TestRun.step("Verify no errors occurred"):
            TestRun.executor.wait_cmd_finish(pid)
            results = fio_run.get_results(
                TestRun.executor.run(f"cat {fio_run.fio.fio_file}").stdout)
            TestRun.LOGGER.info(
                "Recorded " + str(sum(job.read_iops() for job in results)) + " read IOPS")
            TestRun.LOGGER.info(
                "Recorded " + str(sum(job.write_iops() for job in results)) + " write IOPS")
            errors = sum(job.total_errors() for job in results)
            if errors != 0:
                raise Exception("Errors found during io")

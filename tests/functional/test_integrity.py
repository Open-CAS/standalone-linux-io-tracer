#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import random
import datetime
import time

import pytest

from core.test_run import TestRun
from iotrace import IotracePlugin
from utils.fio import run_workload


runtime = datetime.timedelta(seconds=20)


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

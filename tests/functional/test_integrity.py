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
from test_tools.fio.fio import Fio
from test_tools.fio.fio_param import ReadWrite, IoEngine, VerifyMethod
from test_utils.size import Unit, Size


runtime = datetime.timedelta(seconds=20)


def test_data_integrity_20s():
    TestRun.LOGGER.info("Testing data integrity")
    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    for disk in TestRun.dut.disks:
        with TestRun.step("Start tracing"):
            iotrace.start_tracing([disk.system_path])
        with TestRun.step("Run test workloads with verification"):
            setup_workload(disk.system_path)
        with TestRun.step("Stop tracing"):
            iotrace.stop_tracing()


def setup_workload(target):
    fio_run = Fio().create_command()
    fio_run.io_engine(IoEngine.libaio)
    fio_run.direct()
    fio_run.time_based()
    fio_run.do_verify()
    fio_run.verify(VerifyMethod.meta)
    fio_run.verify_dump()
    fio_run.run_time(runtime)
    fio_run.read_write(ReadWrite.randrw)
    fio_run.io_depth(128)
    fio_run.target(target)

    fio_job = fio_run.add_job()
    fio_job.stonewall()
    fio_job.block_size(int(Size(4, Unit.KibiByte)))

    fio_run.run()

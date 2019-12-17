#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from core.test_run import TestRun
from test_tools.fio.fio import Fio
from test_tools.fio.fio_param import ReadWrite, IoEngine, VerifyMethod
from test_utils.size import Unit, Size


def run_workload(target, runtime, io_depth=128, verify=True,
                 block_size=int(Size(4, Unit.KibiByte)), num_jobs=1, method=ReadWrite.randrw):
    fio_run = Fio().create_command()
    fio_run.io_engine(IoEngine.libaio)
    fio_run.direct()
    fio_run.time_based()

    fio_run.run_time(runtime)
    fio_run.io_depth(io_depth)
    if verify:
        fio_run.do_verify()
        fio_run.verify(VerifyMethod.meta)
        fio_run.verify_dump()

    fio_run.read_write(method)
    fio_run.target(target)
    fio_run.block_size(block_size)

    for i in range(num_jobs):
        fio_run.add_job()

    return fio_run.run()

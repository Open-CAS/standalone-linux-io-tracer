#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from core.test_run_utils import TestRun
from utils.installer import install_iotrace, check_if_installed
from utils.iotrace import IotracePlugin
from utils.misc import kill_all_io
from test_tools.fio.fio import Fio


def dut_prepare(reinstall: bool):
    if not check_if_installed() or reinstall:
        TestRun.LOGGER.info("Installing iotrace:")
        install_iotrace()
    else:
        TestRun.LOGGER.info("iotrace is already installed by previous test")

    # Call it after installing iotrace because we need iotrace
    # to get valid paths
    dut_cleanup()

    fio = Fio()
    if not fio.is_installed():
        TestRun.LOGGER.info("Installing fio")
        fio.install()

    TestRun.LOGGER.info("Killing all IO")
    kill_all_io()


def dut_cleanup():
    iotrace: IotracePlugin = TestRun.plugins['iotrace']

    TestRun.LOGGER.info("Stopping fuzzing")
    TestRun.executor.run(f'{iotrace.working_dir}/standalone-linux-io-tracer/tests/security/fuzzy/fuzz.sh clean')

    output = TestRun.executor.run('pgrep iotrace')
    if output.stdout != "":
        TestRun.executor.run(f'kill -9 {output.stdout}')

    TestRun.LOGGER.info("Removing existing traces")
    trace_repository_path: str = iotrace.get_trace_repository_path()
    TestRun.executor.run_expect_success(f'rm -rf {trace_repository_path}/kernel')

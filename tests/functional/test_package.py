#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from core.test_run import TestRun
from utils.iotrace import IotracePlugin
from datetime import datetime, timedelta
from utils.installer import check_if_installed, install_iotrace, uninstall_iotrace


def test_package_installation():
    TestRun.LOGGER.info("Testing package installation")

    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    work_path: str = f"{iotrace.working_dir}/iotrace_package"

    with TestRun.step("Copying iotrace repository to DUT"):
        TestRun.executor.rsync(
            f"{iotrace.repo_dir}/",
            work_path,
            delete=True,
            symlinks=True,
            exclude_list=['build'], timeout=timedelta(minutes=2))

    with TestRun.step("Building iotrace package"):
        TestRun.executor.run_expect_success(
            f"cd {work_path} && "
            "make package -j`nproc --all`")

    iotrace_installed = check_if_installed()
    if iotrace_installed:
        with TestRun.step("Uninstall existing iotrace if needed"):
            uninstall_iotrace()

    with TestRun.step("Install from iotrace package"):
        TestRun.executor.run_expect_success(
            f"cd {work_path}/build/release && "
            "rpm -i --force iotrace-*.rpm")

    with TestRun.step("Check if iotrace is installed"):
        iotrace.version()

    with TestRun.step("Uninstall rpm package"):
        TestRun.executor.run_expect_success("rpm -e iotrace")

    with TestRun.step("Check if io trace was uninstalled"):
        TestRun.executor.run_expect_fail("iotrace -V")

    if iotrace_installed:
        with TestRun.step("Reinstall iotrace"):
            install_iotrace()


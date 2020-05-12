#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import time
from core.test_run import TestRun
from utils.iotrace import IotracePlugin
from datetime import timedelta
from utils.installer import (
    check_if_installed,
    check_if_ubuntu,
    install_iotrace,
    uninstall_iotrace
)


def test_package_installation():
    TestRun.LOGGER.info("Testing package installation")

    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    work_path: str = f"{iotrace.working_dir}/iotrace_package"
    disk = TestRun.dut.disks[0]

    with TestRun.step("Copying iotrace repository to DUT"):
        TestRun.executor.rsync_to(
            f"{iotrace.repo_dir}/",
            work_path,
            delete=True,
            symlinks=True,
            exclude_list=['build'] + ['*.pyc'], timeout=timedelta(minutes=2))

    with TestRun.step("Setup dependencies"):
        TestRun.executor.run_expect_success(
            f"cd {work_path} && "
            "./setup_dependencies.sh")

    with TestRun.step("Remove old packages"):
        TestRun.executor.run_expect_success(f"cd {work_path}/build/release && "
                                            "rm -rf iotrace-*.deb && "
                                            "rm -rf iotrace-*.rpm")

    with TestRun.step("Building iotrace package"):
        TestRun.executor.run_expect_success(
            f"cd {work_path} && "
            "make package -j`nproc --all`")

    iotrace_installed = check_if_installed()
    if iotrace_installed:
        with TestRun.step("Uninstall existing iotrace if needed"):
            uninstall_iotrace()

    with TestRun.step("Install from iotrace package"):
        if check_if_ubuntu():
            TestRun.executor.run_expect_success(
                f"cd {work_path}/build/release && "
                "dpkg -i iotrace-*.deb")
        else:
            TestRun.executor.run_expect_success(
                f"cd {work_path}/build/release && "
                "rpm -i iotrace-*.rpm")

    with TestRun.step("Check if iotrace is installed"):
        iotrace.version()

    iotrace.start_tracing([disk.system_path])
    time.sleep(1)
    stopped = iotrace.stop_tracing()

    if not stopped:
        raise Exception("Could not stop active tracing.")

    trace_path = IotracePlugin.get_latest_trace_path()
    summary_parsed = IotracePlugin.get_trace_summary(trace_path)

    if summary_parsed['state'] != "COMPLETE":
        TestRun.LOGGER.error("Trace state is not complete")

    with TestRun.step("Uninstall rpm package"):
        if check_if_ubuntu():
            TestRun.executor.run_expect_success("apt remove -y iotrace")
        else:
            TestRun.executor.run_expect_success("rpm -e iotrace")

    with TestRun.step("Check if iotrace was uninstalled"):
        TestRun.executor.run_expect_fail("iotrace -V")

    if iotrace_installed:
        with TestRun.step("Reinstall iotrace"):
            install_iotrace()


def test_source_package_installation():
    TestRun.LOGGER.info("Testing source package installation")

    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    work_path: str = f"{iotrace.working_dir}/standalone-linux-io-tracer"
    disk = TestRun.dut.disks[0]

    with TestRun.step("Building iotrace source package"):
        TestRun.executor.run_expect_success(
            f"cd {work_path} && "
            "make package_source -j`nproc --all`")

    iotrace_installed = check_if_installed()
    if iotrace_installed:
        with TestRun.step("Uninstall existing iotrace if needed"):
            uninstall_iotrace()

    with TestRun.step("Remove old source package"):
        TestRun.executor.run_expect_success(f"cd {work_path} && "
                                            f"rm -rf iotrace-*-Source")

    with TestRun.step("Unpack and install source package"):
        TestRun.executor.run_expect_success(
            f"tar -xvf {work_path}/build/release/iotrace-*.tar.gz -C {work_path} &&"
            f"cd {work_path}/iotrace-*-Source &&"
            "./setup_dependencies.sh &&"
            "make install -j`nproc --all`")

    with TestRun.step("Check if iotrace is installed"):
        iotrace.version()

    iotrace.start_tracing([disk.system_path])
    stopped = iotrace.stop_tracing()

    if not stopped:
        raise Exception("Could not stop active tracing.")

    trace_path = IotracePlugin.get_latest_trace_path()
    summary_parsed = IotracePlugin.get_trace_summary(trace_path)

    if summary_parsed['state'] != "COMPLETE":
        TestRun.LOGGER.error("Trace state is not complete")

    with TestRun.step("Uninstall iotrace"):
        uninstall_iotrace()

    with TestRun.step("Check if iotrace was uninstalled"):
        TestRun.executor.run_expect_fail("iotrace -V")

    if iotrace_installed:
        with TestRun.step("Reinstall iotrace"):
            install_iotrace()

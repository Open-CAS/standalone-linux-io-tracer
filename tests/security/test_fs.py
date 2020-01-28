#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from core.test_run import TestRun
import time


def test_files_privileges():
    TestRun.LOGGER.info("Testing iotrace config owner and trace file privileges")
    iotrace = TestRun.plugins['iotrace']

    # Find paths in manifest since their location is OS dependent
    out = TestRun.executor.run_expect_success('find /usr -name install_manifest_octf-install.txt'
                                              ' | xargs cat | grep octf.conf$')
    config_path = out.stdout
    if config_path == "":
        raise Exception("Could not find octf config path")

    out = TestRun.executor.run_expect_success(f"cat {config_path}")
    config = iotrace.parse_json(out.stdout)

    TestRun.LOGGER.info(f"Checking permissions of: {config_path}")
    out = TestRun.executor.run_expect_success(f'stat -c "%a" {config_path}')
    config_privileges = out.stdout
    if config_privileges != "644":
        TestRun.fail(f"Invalid permissions on config file, found: {config_privileges}")

    out = TestRun.executor.run_expect_success(f'stat -c "%U" {config_path}')
    config_owner = out.stdout
    if config_owner != "root":
        TestRun.fail("Invalid owner of config file")

    TestRun.LOGGER.info("Testing trace files privileges")

    # Make sure trace dir exists
    iotrace.start_tracing()
    iotrace.stop_tracing()

    # Check permissions of all files inside trace repository
    trace_repo = config[0]['paths']['trace']
    out = TestRun.executor.run_expect_success(f'find {trace_repo}/ -name "*"')
    file_list = str.splitlines(out.stdout)

    for file in file_list:
        out = TestRun.executor.run_expect_success(f'stat -c "%a" {file}')
        file_privileges = out.stdout

        out = TestRun.executor.run(f'test -d {file}')
        is_dir = not bool(out.exit_code)

        # Directory
        if is_dir:
            if file_privileges != "750":
                TestRun.fail(f"Invalid permissions in file: {file}")

        # Regular file
        else:
            if file_privileges != "440":
                TestRun.fail(f"Invalid permissions in file: {file}")

        TestRun.LOGGER.info(f"Correct permissions for file {file}")


def test_procfs_privileges():
    TestRun.LOGGER.info("Testing procfs file privileges")
    iotrace = TestRun.plugins['iotrace']

    # Lists of files to check with their permissions
    procfs_files = {"devices": "400",
                    "version": "400",
                    "remove_device": "200",
                    "add_device": "200",
                    "size": "600"}
    procfs_files_per_cpu = {"trace_ring": "400",
                            "consumer_hdr": "600"}
    procfs_path_prefix = "/proc/iotrace/"

    cpu_count = int(TestRun.executor.run_expect_success('nproc').stdout)
    procfiles_count = len(procfs_files) + len(procfs_files_per_cpu) * cpu_count

    # Start tracing
    iotrace.start_tracing()
    time.sleep(1)

    # Check file count in procfs
    expected_file_count = int(TestRun.executor.run_expect_success(
        'ls -1 /proc/iotrace | wc -l').stdout)
    if expected_file_count > procfiles_count:
        TestRun.fail(f"Extra files files found in procfs: found"
                     f" {procfiles_count} procfiles,")

    # Check singular files
    for file, permissions in procfs_files.items():
        file_path = procfs_path_prefix + file
        TestRun.LOGGER.info(f"Checking permissions of: {file_path}")
        out = TestRun.executor.run(f'stat -c "%a" {file_path}')
        file_permissions = out.stdout

        if out.exit_code != 0:
            TestRun.fail(f"Can't find file: {file_path}")

        if file_permissions != permissions:
            TestRun.fail(f"Invalid permissions on {file_path}, found {file_permissions},"
                         f" expected {permissions}")

    # Check per cpu files
    for file, permissions in procfs_files_per_cpu.items():
        for cpu in range(cpu_count):
            file_path = procfs_path_prefix + file + "." + str(cpu)
            TestRun.LOGGER.info(f"Checking permissions of: {file_path}")
            out = TestRun.executor.run(f'stat -c "%a" {file_path}')
            file_permissions = out.stdout

            if out.exit_code != 0:
                TestRun.fail(f"Can't find file: {file_path}")

            if file_permissions != permissions:
                TestRun.fail(f"Invalid permissions on {file_path}, found {file_permissions},"
                             f" expected {permissions}")

    result = iotrace.stop_tracing()
    if not result:
        TestRun.fail("Tracing process not found")

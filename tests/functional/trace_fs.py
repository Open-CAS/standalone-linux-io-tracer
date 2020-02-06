#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from core.test_run import TestRun
from test_tools.disk_utils import Filesystem
from test_utils.os_utils import sync
from test_tools.fs_utils import create_directory, create_file, move, remove, \
    write_file, ls

mountpoint = "/mnt"


def get_inode(file):
    file = ls(file, '-i')
    return file.split(' ')[0]


def test_fs_operations():
    TestRun.LOGGER.info("Testing file system events during tracing")
    iotrace = TestRun.plugins['iotrace']

    for disk in TestRun.dut.disks:
        try:
            with TestRun.step("Create file system"):
                disk.create_filesystem(Filesystem.ext4)
            with TestRun.step("Start tracing"):
                iotrace.start_tracing([disk.system_path])
            with TestRun.step("Mount device"):
                disk.mount(mountpoint)
            with TestRun.step("Create test directory and file"):
                write_file(f"{mountpoint}/test_file", content="foo")
                sync()
                test_file_inode = get_inode(f"{mountpoint}/test_file")
                create_directory(f"{mountpoint}/test_dir")
                sync()
            with TestRun.step("Write to test file"):
                write_file(f"{mountpoint}/test_file", overwrite=False,
                           content="bar")
                sync()
            with TestRun.step("Create new test file"):
                create_file(f"{mountpoint}/test_file2")
                test_file2_inode = get_inode(f"{mountpoint}/test_file2")
                sync()
            with TestRun.step("Move test file"):
                move(f"{mountpoint}/test_file", f"{mountpoint}/test_dir")
                sync()
            with TestRun.step("Delete test file"):
                remove(f"{mountpoint}/test_dir/test_file")
                sync()
            with TestRun.step("Stop tracing"):
                sync()
                iotrace.stop_tracing()
            with TestRun.step("Verify trace correctness"):
                trace_path = iotrace.get_latest_trace_path()
                events = iotrace.get_trace_events(trace_path)
                events_parsed = iotrace.parse_json(events)
                result = any(
                    'file' in log and log['file']['eventType'] == 'Create' and
                    log['file']['id'] == test_file2_inode for log in events_parsed)
                if not result:
                    raise Exception("Could not find Create event")
                result = any(
                    'file' in log and log['file']['eventType'] == 'Delete' and
                    log['file']['id'] == test_file_inode for log in events_parsed)
                if not result:
                    raise Exception("Could not find Delete event")
                result = any(
                    'file' in log and log['file']['eventType'] == 'MoveTo' and
                    log['file']['id'] == test_file_inode for log in events_parsed)
                if not result:
                    raise Exception("Could not find MoveTo event")
                result = any(
                    'file' in log and log['file']['eventType'] == 'MoveFrom' and
                    log['file']['id'] == test_file_inode for log in events_parsed)
                if not result:
                    raise Exception("Could not find MoveFrom event")
                result = any(
                    'file' in log and log['file']['eventType'] == 'Access' and
                    log['file']['id'] == test_file_inode for log in events_parsed)
                if not result:
                    raise Exception("Could not find Access event")
        finally:
            with TestRun.step("Unmount device"):
                disk.unmount()

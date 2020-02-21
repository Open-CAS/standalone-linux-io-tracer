#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from core.test_run import TestRun
from utils.iotrace import IotracePlugin
from test_tools.disk_utils import Filesystem
from test_utils.os_utils import sync
from test_tools.fs_utils import create_directory, create_file, move, remove, \
    write_file, ls
import time

mountpoint = "/mnt"


def test_iotracer_multiple_instances():
    """
        title: Check if it’s possible to run concurrent tracing processes.
        description: |
          Try to run multiple concurrent io-tracers for the same disks.
        pass_criteria:
          - No system crash.
          - It’s impossible to run io-tracer multiple times for the same disks.
    """
    with TestRun.step("Run the first io-tracer instance"):
        iotracer1 = IotracePlugin(TestRun.plugins['iotrace'].repo_dir,
                                  TestRun.plugins['iotrace'].working_dir)
        iotracer1.start_tracing()

    with TestRun.step("Check if the first instance of io-tracer works."):
        if not iotracer1.check_if_tracing_active():
            TestRun.fail("There is no working io-tracer instance.")
        TestRun.LOGGER.info(f"Iotrace process found with PID {iotracer1.pid}")

    with TestRun.step("Try to start the second iotracer instance."):
        iotracer2 = IotracePlugin(TestRun.plugins['iotrace'].repo_dir,
                                  TestRun.plugins['iotrace'].working_dir)
        iotracer2.start_tracing()
        if iotracer2.check_if_tracing_active():
            TestRun.fail("There is working iotracer second instance.")
        TestRun.LOGGER.info(
            "Cannot start the second instance of iotracer as expected.")

    with TestRun.step("Stop io-tracer."):
        iotracer1.stop_tracing()


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
            with TestRun.step("Mount device"):
                disk.mount(mountpoint)
            with TestRun.step("Start tracing"):
                iotrace.start_tracing([disk.system_path])
                time.sleep(5)
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
                events = IotracePlugin.get_trace_events(trace_path)
                events_parsed = iotrace.parse_json(events)
                result = any(
                    'file' in event and event['file']['eventType'] == 'Create'
                    and event['file']['id'] == test_file2_inode
                    for event in events_parsed)
                if not result:
                    raise Exception("Could not find Create event")
                result = any(
                    'file' in event and event['file']['eventType'] == 'Delete'
                    and event['file']['id'] == test_file_inode
                    for event in events_parsed)
                if not result:
                    raise Exception("Could not find Delete event")
                result = any(
                    'file' in event and event['file']['eventType'] == 'MoveTo'
                    and event['file']['id'] == test_file_inode
                    for event in events_parsed)
                if not result:
                    raise Exception("Could not find MoveTo event")
                result = any(
                    'file' in event and event['file']['eventType']
                    == 'MoveFrom'
                    and event['file']['id'] == test_file_inode
                    for event in events_parsed)
                if not result:
                    raise Exception("Could not find MoveFrom event")
                result = any(
                    'file' in event and event['file']['eventType'] == 'Access'
                    and event['file']['id'] == test_file_inode
                    for event in events_parsed)
                if not result:
                    raise Exception("Could not find Access event")
        finally:
            with TestRun.step("Unmount device"):
                disk.unmount()

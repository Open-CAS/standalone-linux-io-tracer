#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from time import sleep
import pytest

from core.test_run import TestRun
from test_tools.fs_utils import remove, read_file, ls
from utils.iotrace import IotracePlugin, parse_json

custom_repo_path = "/var/lib/octf/custom_trace"
config_file = "/etc/octf/octf.conf"


@pytest.mark.parametrize("use_short_cmd", [False, True])
def test_get_repo_path(use_short_cmd):
    """
        title: Test of iotrace -C -G functionality.
        description: |
          Verify the output of 'iotrace -C -G' is proper.
        pass_criteria:
          - Getting path to trace repository returns proper path.
    """
    with TestRun.step("Get trace repository path from OCTF config file."):
        default_trace_repo_path = parse_json(read_file(config_file))[0]["paths"]["trace"]

    with TestRun.step("Get trace repository path by command."):
        trace_repo_path = IotracePlugin.get_trace_repository_path(use_short_cmd)

    with TestRun.step("Compare with default path"):
        if trace_repo_path != default_trace_repo_path:
            TestRun.LOGGER.error(f'Command output should be {default_trace_repo_path}\n'
                                 f'but its actually {trace_repo_path}')


@pytest.mark.parametrize("use_short_cmd", [False, True])
def test_set_repo_path(use_short_cmd):
    """
        title: Test of iotrace -C -S functionality.
        description: |
          Verify the command 'iotrace -C -S' properly sets new path.
        pass_criteria:
          - Setting new path to trace repository receives proper path.
    """
    with TestRun.step("Check current trace repository path."):
        previous_trace_repo_path = parse_json(read_file(config_file))[0]["paths"]["trace"]

    with TestRun.step("Set new path to trace repository."):
        IotracePlugin.set_trace_repository_path(custom_repo_path, use_short_cmd)

    with TestRun.step("Check if path has actually changed."):
        if previous_trace_repo_path == IotracePlugin.get_trace_repository_path():
            TestRun.LOGGER.error("Trace repository path hasn't changed.")

    with TestRun.step("Start short tracing with new path."):
        TestRun.plugins["iotrace"].start_tracing(timeout=8)
        sleep(9)

    with TestRun.step("Check if tracing output is saved in proper place."):
        if not ls(custom_repo_path):
            TestRun.LOGGER.error("Iotracer doesn't save files in custom repository path")

    with TestRun.step("Set back trace repository path."):
        IotracePlugin.set_trace_repository_path(previous_trace_repo_path)
        if previous_trace_repo_path != IotracePlugin.get_trace_repository_path():
            TestRun.LOGGER.error("Trace repository path hasn't changed back.")

    with TestRun.step("Remove custom test directory."):
        remove(custom_repo_path, True, True)

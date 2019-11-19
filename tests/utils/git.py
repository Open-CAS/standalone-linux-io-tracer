#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from connection.local_executor import LocalExecutor
from log.logger import create_log, Log
from core.test_run_utils import TestRun

def get_current_commit_hash():
    local_executor = LocalExecutor()
    print(TestRun.plugins['iotrace'].repo_dir)
    return local_executor.run(
        "cd " + TestRun.plugins['iotrace'].repo_dir + " && "
        + 'git show HEAD -s --pretty=format:"%H"').stdout


def get_current_commit_message():
    local_executor = LocalExecutor()
    return local_executor.run(
        f"cd {TestRun.plugins['iotrace'].repo_dir} &&"
        f'git show HEAD -s --pretty=format:"%B"').stdout
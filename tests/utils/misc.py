#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from core.test_run_utils import TestRun


def kill_all_io():
    TestRun.executor.run("pkill --signal SIGKILL dd")
    TestRun.executor.run(
        "kill -9 `ps aux | grep -i vdbench.* | awk '{ print $1 }'`")
    TestRun.executor.run("pkill --signal SIGKILL fio*")
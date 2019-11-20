#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from core.test_run import TestRun


def test_help():
    TestRun.LOGGER.info("Testing cli help")
    output = TestRun.executor.run('iotrace -H')
    if output.exit_code != 0:
        raise Exception("Failed to run executable")


def test_version():
    TestRun.LOGGER.info("Testing cli version")
    output = TestRun.executor.run('iotrace -V')
    if output.exit_code != 0:
        raise Exception("Failed to run executable")


def test_module_loaded():
    TestRun.LOGGER.info("Testing iotrace kernel module loading")
    output = TestRun.executor.run('lsmod | grep iotrace')
    if output.exit_code != 0:
        raise Exception("Failed to find iotrace kernel module")

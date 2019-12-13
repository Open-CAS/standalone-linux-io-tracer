#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from core.test_run import TestRun
from test_tools import dd


def test_help():
    TestRun.LOGGER.info("Testing help")
    output = TestRun.executor.run('iotrace -H')
    if output.exit_code != 0:
        raise Exception("Failed to run executable")
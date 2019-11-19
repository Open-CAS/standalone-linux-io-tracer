#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from core.test_run import TestRun

def test_example1():
        TestRun.LOGGER.info("Test run")

def test_example2():
        TestRun.LOGGER.info("Test run2")
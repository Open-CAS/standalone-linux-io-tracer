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

    parsed = TestRun.plugins['iotrace'].parse_output(output.stdout)
    bin_version = parsed[0]['trace'].split()[0]

    TestRun.LOGGER.info("iotrace binary version is: " + str(parsed[0]['trace']))
    TestRun.LOGGER.info("OCTF library version is: " + str(parsed[0]['trace']))

    output = TestRun.executor.run("dmesg | grep iotrace | tail -n 1")
    if output.exit_code != 0:
        raise Exception("Could not find module version")
    module_version = output.stdout.split()[-1]

    TestRun.LOGGER.info("Module version is: " + module_version)

    if bin_version != module_version:
        raise Exception("Mismatching executable and module versions")


def test_module_loaded():
    TestRun.LOGGER.info("Testing iotrace kernel module loading")
    output = TestRun.executor.run('lsmod | grep iotrace')
    if output.exit_code != 0:
        raise Exception("Failed to find loaded iotrace kernel module")

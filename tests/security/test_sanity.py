#
# Copyright(c) 2019-2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

import pytest
import time
from core.test_run import TestRun
from utils.iotrace import IotracePlugin
from utils.installer import insert_module
from utils.iotrace import parse_json


def test_help():
    TestRun.LOGGER.info("Testing cli help")
    output = TestRun.executor.run('iotrace -H')
    if output.exit_code != 0:
        raise Exception("Failed to run executable")


def test_version():
    # Make sure module is loaded
    insert_module()
    TestRun.LOGGER.info("Testing cli version")
    output = TestRun.executor.run('iotrace -V')

    parsed = parse_json(output.stdout)
    bin_version = parsed[0]['trace']

    TestRun.LOGGER.info("iotrace binary version is: " + str(parsed[0]['trace']))
    TestRun.LOGGER.info("OCTF library version is: " + str(parsed[1]['trace']))

    output = TestRun.executor.run("cat /sys/module/iotrace/version")
    if output.exit_code != 0:
        raise Exception("Could not find module version")
    module_version = output.stdout

    TestRun.LOGGER.info("Module version is: " + module_version)
    if bin_version != module_version:
        raise Exception("Mismatching executable and module versions")


def test_module_loaded():
    # Make sure module is loaded
    insert_module()

    TestRun.LOGGER.info("Testing iotrace kernel module loading")
    output = TestRun.executor.run('lsmod | grep iotrace')
    if output.exit_code != 0:
        raise Exception("Failed to find loaded iotrace kernel module")


def test_trace_start_stop():
    TestRun.LOGGER.info("Testing starting and stopping of tracing")
    iotrace: IotracePlugin = TestRun.plugins['iotrace']

    iotrace.start_tracing()
    time.sleep(1)
    stopped = iotrace.stop_tracing()

    if not stopped:
        raise Exception("Could not stop active tracing.")

    trace_path = IotracePlugin.get_latest_trace_path()
    summary_parsed = IotracePlugin.get_trace_summary(trace_path)

    if summary_parsed['state'] != "COMPLETE":
        raise Exception("Trace state is not complete")


# TODO (trybicki) test for sanity checking installation, e.g. validating install_manifest.

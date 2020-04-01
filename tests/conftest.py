import pytest
import os
import sys

# Path to test-framework
sys.path.append(os.path.join(os.path.dirname(__file__),
                             "../modules/test-framework"))

from core.test_run_utils import TestRun
from utils.iotrace import IotracePlugin
from utils.dut import dut_cleanup, dut_prepare


# Called for each test in directory
def pytest_runtest_setup(item):
    iotrace: IotracePlugin = IotracePlugin()
    TestRun.plugins['iotrace'] = iotrace
    iotrace.runtest_setup(item)
    dut_prepare(item.config.getoption('--force-reinstall'))


@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item, call):
    res = (yield).get_result()
    TestRun.makereport(item, call, res)


def pytest_runtest_teardown():
    """
    This method is executed always in the end of each test,
    even if it fails or raises exception in prepare stage.
    """
    if TestRun.outcome == "skipped":
        return

    dut_cleanup()

    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    iotrace.runtest_teardown()


def pytest_addoption(parser):
    parser.addoption("--dut-config", action="store", default="None")
    parser.addoption("--log-path", action="store", default=".")
    parser.addoption("--force-reinstall", action="store_true")

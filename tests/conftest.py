import pytest
import os
import sys
import yaml
import traceback

# Path to test-framework
sys.path.append(os.path.join(os.path.dirname(__file__),
                             "../modules/test-framework"))
# TODO (trybicki) Don't use file relative paths if possible
sys.path.append(os.path.join(os.path.dirname(__file__), "./utils"))

from log.logger import create_log, Log
from core.test_run_utils import TestRun
from utils.git import get_current_commit_hash, get_current_commit_message
from utils.git import get_current_octf_hash
from utils.installer import install_iotrace
from utils.misc import kill_all_io
from utils.iotrace import IotracePlugin
from test_tools.fio.fio import Fio


# Called for each test in directory
def pytest_runtest_setup(item):
    try:
        with open(item.config.getoption('--dut-config')) as cfg:
            dut_config = yaml.safe_load(cfg)
    except Exception:
        raise Exception("You need to specify DUT config. "
                        "See the example_dut_config.py file.")

    try:
        TestRun.prepare(item, dut_config)

        test_name = item.name.split('[')[0]
        TestRun.LOGGER = create_log(item.config.getoption('--log-path'), test_name)

        TestRun.setup()

        TestRun.plugins['iotrace'] = IotracePlugin(
            repo_dir=os.path.join(os.path.dirname(__file__), "../"),
            working_dir=dut_config['working_dir']
        )
    except Exception as ex:
        raise Exception(f"Exception occurred during test setup:\n"
                        f"{str(ex)}\n{traceback.format_exc()}")

    TestRun.LOGGER.info(f"DUT info: {TestRun.dut}")

    # Prepare DUT for test
    TestRun.LOGGER.add_build_info(f'Commit hash:')
    TestRun.LOGGER.add_build_info(f"{get_current_commit_hash()}")
    TestRun.LOGGER.add_build_info(f'Commit message:')
    TestRun.LOGGER.add_build_info(f'{get_current_commit_message()}')
    TestRun.LOGGER.add_build_info(f'OCTF commit hash:')
    TestRun.LOGGER.add_build_info(f'{get_current_octf_hash()}')

    dut_prepare(item.config.getoption('--force-reinstall'))

    TestRun.LOGGER.info(f"DUT info: {TestRun.dut}")
    TestRun.LOGGER.write_to_command_log("Test body")
    TestRun.LOGGER.start_group("Test body")


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

    TestRun.LOGGER.end_all_groups()

    with TestRun.LOGGER.step("Cleanup after test"):
        try:
            if TestRun.executor.is_active():
                TestRun.executor.wait_for_connection()

        except Exception:
            TestRun.LOGGER.warning("Exception occured during platform cleanup.")

    dut_cleanup()
    TestRun.LOGGER.end()
    if TestRun.executor:
        TestRun.LOGGER.get_additional_logs()
    Log.destroy()
    TestRun.teardown()


def pytest_addoption(parser):
    parser.addoption("--dut-config", action="store", default="None")
    parser.addoption("--log-path", action="store", default=".")
    parser.addoption("--force-reinstall", action="store_true")


def dut_prepare(force_reinstall):
    if not TestRun.plugins['iotrace'].installed or force_reinstall:
        TestRun.LOGGER.info("Installing iotrace:")
        install_iotrace()
    else:
        TestRun.LOGGER.info("iotrace is already installed by previous test")

    # Call it after installing iotrace because we need iotrace
    # to get valid paths
    dut_cleanup()

    fio = Fio()
    if not fio.is_installed():
        TestRun.LOGGER.info("Installing fio")
        fio.install()

    TestRun.LOGGER.info("Killing all IO")
    kill_all_io()


def dut_cleanup():
    iotrace: IotracePlugin = TestRun.plugins['iotrace']

    TestRun.LOGGER.info("Stopping fuzzing")
    TestRun.executor.run(f'{iotrace.working_dir}/tests/security/fuzzy/fuzz.sh clean')

    iotrace.stop_tracing()

    TestRun.LOGGER.info("Removing existing traces")
    trace_repository_path: str = IotracePlugin.get_trace_repository_path()
    TestRun.executor.run_expect_success(
        f'rm -rf {trace_repository_path}/kernel')

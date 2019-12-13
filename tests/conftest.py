import pytest
import os
import sys
import yaml
import traceback
from IPy import IP

# Path to test-framework
sys.path.append(os.path.join(os.path.dirname(__file__),
                             "../modules/test-framework"))
# TODO (trybicki) Don't use file relative paths if possible
sys.path.append(os.path.join(os.path.dirname(__file__), "./utils"))

from log.logger import create_log, Log
from core.test_run_utils import TestRun
from connection.local_executor import LocalExecutor
from utils.git import get_current_commit_hash, get_current_commit_message
from utils.git import get_current_octf_hash
from utils.installer import install_iotrace, uninstall_iotrace
from utils.installer import remove_module
from utils.misc import kill_all_io
from utils.iotrace import IotracePlugin
from test_tools.fio.fio import Fio


# Called for each test in directory
def pytest_runtest_setup(item):

    TestRun.prepare(item)

    test_name = item.name.split('[')[0]
    TestRun.LOGGER = create_log(item.config.getoption('--log-path'), test_name)

    with TestRun.LOGGER.step("Test initialization"):

        try:
            # Open and parse yaml config file
            try:
                with open(item.config.getoption('--dut-config')) as cfg:
                    dut_config = yaml.safe_load(cfg)

            except Exception as e:
                print(e)
                exit(1)

            # Setup from dut config
            TestRun.setup(dut_config)

            TestRun.plugins['iotrace'] = IotracePlugin(
                repo_dir=os.path.join(os.path.dirname(__file__), "../"),
                working_dir=dut_config['working_dir']
            )

        except Exception as e:
            TestRun.LOGGER.exception(f"{str(e)}\n{traceback.format_exc()}")

        TestRun.LOGGER.info(f"DUT info: {TestRun.dut}")

    # Prepare DUT for test
    with TestRun.LOGGER.step("DUT prepare"):
        TestRun.LOGGER.add_build_info(f'Commit hash:')
        TestRun.LOGGER.add_build_info(f"{get_current_commit_hash()}")
        TestRun.LOGGER.add_build_info(f'Commit message:')
        TestRun.LOGGER.add_build_info(f'{get_current_commit_message()}')
        TestRun.LOGGER.add_build_info(f'OCTF commit hash:')
        TestRun.LOGGER.add_build_info(f'{get_current_octf_hash()}')
        dut_prepare(item)

    TestRun.LOGGER.start_group("Test body")


def pytest_runtest_teardown():
    """
    This method is executed always in the end of each test,
    even if it fails or raises exception in prepare stage.
    """

    TestRun.LOGGER.end_all_groups()

    with TestRun.LOGGER.step("Cleanup after test"):
        try:
            if TestRun.executor.is_active():
                TestRun.executor.wait_for_connection()

        except Exception:
            TestRun.LOGGER.warning("Exception occured during platform cleanup.")

    TestRun.LOGGER.end()
    Log.destroy()


def pytest_addoption(parser):
    parser.addoption("--dut-config", action="store", default="None")
    parser.addoption("--log-path", action="store", default=".")


def dut_prepare(item):
    if not TestRun.plugins['iotrace'].installed:
        TestRun.LOGGER.info("Installing iotrace")
        install_iotrace()
    else:
        TestRun.LOGGER.info("iotrace is already installed by previous test")

    TestRun.LOGGER.info("Killing all IO")
    kill_all_io()

    fio = Fio()
    if not fio.is_installed():
        TestRun.LOGGER.info("Installing fio")
        fio.install()

    TestRun.plugins['iotrace'].stop_tracing()


def dut_cleanup(item):
    TestRun.LOGGER.info("Removing iotrace module")
    remove_module()

import pytest
import os
import sys
import yaml
import traceback
from IPy import IP

# Path to test-framework
sys.path.append(os.path.join(os.path.dirname(__file__), "../modules/test-framework"))
sys.path.append(os.path.join(os.path.dirname(__file__), "../utils"))

from log.logger import create_log, Log
from core.test_run_utils import TestRun
from connection.local_executor import LocalExecutor
from utils.git import get_current_commit_hash, get_current_commit_message
from utils.installer import install_iotrace, uninstall_iotrace
from test_utils.singleton import Singleton


# Flag to stop test teardown if test setup has failed
setup_complete = False

"""
Singleton class to provide test-session wide scope
"""
class IotracePlugin(metaclass=Singleton):
    def __init__(self, repo_dir, working_dir):
        self.repo_dir = repo_dir        # Test controller's repo, copied to DUT
        self.working_dir = working_dir  # DUT's make/install work directory
        self.installed = False          # Was iotrace installed already


# Called for each test in directory
def pytest_runtest_setup(item):
    """
    pytest should be executed with option --dut-config=conf_name'.
    and optionally --log-path="./mylogs"
    
    'ip' field should be filled with valid IP string to use remote ssh executor
    or it should be commented out when user want to execute tests on local machine
    """

    # This mostly remembers which disks are required for this test
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

    dut_prepare(item)

    TestRun.LOGGER.write_to_command_log("Test body")
    TestRun.LOGGER.start_group("Test body")
    setup_complete = True


def pytest_runtest_teardown():
    """
    This method is executed always in the end of each test,
    even if it fails or raises exception in prepare stage.
    """
    if not setup_complete :
        print("Setup didnt complete successfully")
        return

    TestRun.LOGGER.end_all_groups()

    with TestRun.LOGGER.step("Cleanup after test"):
        try:
            if TestRun.executor.is_active():
                TestRun.executor.wait_for_connection()

        except Exception:
            TestRun.LOGGER.warning("Exception occured during platform cleanup.")


    TestRun.LOGGER.end()
    TestRun.LOGGER.get_additional_logs()
    Log.destroy()

def pytest_addoption(parser):
    parser.addoption("--dut-config", action="store", default="None")
    parser.addoption("--log-path", action="store", default=".")

def kill_all_io():
    TestRun.executor.run("pkill --signal SIGKILL dd")
    TestRun.executor.run("kill -9 `ps aux | grep -i vdbench.* | awk '{ print $1 }'`")
    TestRun.executor.run("pkill --signal SIGKILL fio*")

    
def dut_prepare(item):
    if not TestRun.plugins['iotrace'].installed:
        install_iotrace()
    kill_all_io()

#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from core.test_run import TestRun
from utils.afl import is_afl_installed, install_afl
from installer import install_iotrace_with_afl_support
from utils.iotrace import IotracePlugin
import time


def test_fuzz_args():
    #TODO 15 mintues
    fuzzing_time_seconds = 10 * 60
    TestRun.LOGGER.info("Fuzzing iotrace args")
    iotrace: IotracePlugin = TestRun.plugins['iotrace']

    repo_path: str = f"{iotrace.working_dir}/slit-afl"

    # Make sure AFL is installed
    if not is_afl_installed():
        install_afl()

    # Install iotrace locally with AFL support
    # Patch so that we redirect fuzzed stdin to argv
    install_iotrace_with_afl_support(
        repo_path + "/tests/functional/fuzzy/redirect-fuzz-to-argv.patch")

    # Instruct the system to output coredumps as files instead of sending them
    # to a specific crash handler app
    TestRun.LOGGER.info('Setting up system for fuzzing')
    TestRun.executor.run_expect_success('echo core > /proc/sys/kernel/core_pattern')
    TestRun.executor.run_expect_success(f'cd {repo_path} && mkdir -p afl-i afl-o')

    # Add input seed which shall be mutated
    TestRun.executor.run_expect_success(f'cd {repo_path} && echo "-L" > afl-i/case0')

    # Run script which will launch fuzzers in separate 'screen'
    # windows in the background
    TestRun.LOGGER.info('Starting fuzzing. This should take ' +
                        str(fuzzing_time_seconds/60) + ' minutes')
    TestRun.executor.run(f'cd {repo_path} && ./tests/functional/fuzzy/fuzz.sh')

    elapsed = 0
    start_time = time.time()
    while elapsed < fuzzing_time_seconds:
        output = TestRun.executor.run_expect_success(
            f'cd {repo_path} && sleep 1 && afl-whatsup afl-o')
        time.sleep(20)
        current_time = time.time()
        elapsed = current_time - start_time

    TestRun.LOGGER.info('Killing fuzzers')
    TestRun.executor.run(f'cd repo_path && ./tests/functional/fuzzy/fuzz.sh clean')
    TestRun.LOGGER.info(output.stdout)

    # Detect crashes
    if 'Crashes found : 0' not in output.stdout:
        TestRun.fail("Crashes during fuzzing were found")

#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from core.test_run import TestRun
from utils.afl import is_afl_installed, install_afl
from installer import install_iotrace_with_afl_support
from utils.iotrace import IotracePlugin


def test_fuzz_args():
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

    TestRun.executor.run(f'cd repo_path && mkdir afl-i afl-o')

    TestRun.executor.run(f'cd repo_path && ./tests/functional/fuzzy/fuzz.sh')
    # Set time to run, output every n seconds to log
    TestRun.executor.run(f'cd repo_path && ./tests/functional/fuzzy/fuzz.sh clean')
    # Detect crashes
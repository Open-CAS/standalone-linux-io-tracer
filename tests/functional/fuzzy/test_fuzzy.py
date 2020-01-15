#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from core.test_run import TestRun
from utils.afl import is_afl_installed, install_afl
from utils.afl import create_patch_redirect_fuzz_to_file
from installer import install_iotrace_with_afl_support
from utils.iotrace import IotracePlugin
import time


def test_fuzz_args():
    fuzzing_time_seconds = 60 * 60
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

    # Run script which will launch parallel fuzzers in separate 'screen'
    # windows in the background
    TestRun.LOGGER.info('Starting fuzzing. This should take ' +
                        str(fuzzing_time_seconds/60) + ' minutes')
    TestRun.executor.run(f'cd {repo_path} && ./tests/functional/fuzzy/fuzz.sh '
                         'rootfs/bin/iotrace')

    # Wait for fuzzing completion and output logs
    output = wait_for_completion(fuzzing_time_seconds, repo_path)

    TestRun.LOGGER.info('Killing fuzzers')
    TestRun.executor.run(f'cd {repo_path} && ./tests/functional/fuzzy/fuzz.sh clean')

    detect_crashes(output.stdout)


def test_fuzz_config():
    fuzzing_time_seconds = 20 * 60
    TestRun.LOGGER.info("Fuzzing iotrace config file")
    iotrace: IotracePlugin = TestRun.plugins['iotrace']

    repo_path: str = f"{iotrace.working_dir}/slit-afl"

    # Make sure AFL is installed
    if not is_afl_installed():
        install_afl()

    # Create patch file for redirecting fuzzed stdin to config file path
    new_patch_path: str = f'{iotrace.working_dir}/redirect_to_config.patch'
    create_patch_redirect_fuzz_to_file(f'{repo_path}/rootfs/etc/octf/octf.conf',
                                       new_patch_path)

    # Install iotrace locally with AFL support and redirect patch
    install_iotrace_with_afl_support(new_patch_path)

    # Instruct the system to output coredumps as files instead of sending them
    # to a specific crash handler app
    TestRun.LOGGER.info('Setting up system for fuzzing')
    TestRun.executor.run_expect_success('echo core > /proc/sys/kernel/core_pattern')
    TestRun.executor.run_expect_success(f'cd {repo_path} && mkdir -p afl-i afl-o')

    # Use config as seed to be mutated
    TestRun.executor.run_expect_success(f'cp {repo_path}/rootfs/etc/octf/octf.conf'
                                        f' {repo_path}/afl-i/case0')

    TestRun.LOGGER.info('Starting fuzzing. This should take ' +
                        str(fuzzing_time_seconds / 60) + ' minutes')

    TestRun.LOGGER.info("Trying 'list-traces' command")
    TestRun.executor.run(f'cd {repo_path} && ./tests/functional/fuzzy/fuzz.sh '
                         '"rootfs/bin/iotrace -L" --one-job')
    output = wait_for_completion(fuzzing_time_seconds/2, repo_path)
    TestRun.executor.run(f'cd {repo_path} && ./tests/functional/fuzzy/fuzz.sh clean')
    detect_crashes(output.stdout)

    TestRun.LOGGER.info("Trying 'get-trace-repository-path' command")
    TestRun.executor.run(f'cd {repo_path} && ./tests/functional/fuzzy/fuzz.sh '
                         '"rootfs/bin/iotrace -L" --one-job')
    output = wait_for_completion(fuzzing_time_seconds/2, repo_path)
    TestRun.executor.run(f'cd {repo_path} && ./tests/functional/fuzzy/fuzz.sh clean')
    detect_crashes(output.stdout)


# TODO: use as AFL plugin which handles starting, stopping and logging
def wait_for_completion(fuzzing_time_seconds: int, repo_path: str, ):
    elapsed = 0
    start_time = time.time()
    while elapsed < fuzzing_time_seconds:
        output = TestRun.executor.run_expect_success(
            f'cd {repo_path} && sleep 1 && afl-whatsup afl-o')
        time.sleep(20)
        current_time = time.time()
        elapsed = current_time - start_time

    # Return last output
    return output


def detect_crashes(fuzz_output: str):
    if 'Crashes found : 0' not in fuzz_output:
        TestRun.fail("Crashes during fuzzing were found. Please find the"
                     ' inputs which cause it in afl-o directory')


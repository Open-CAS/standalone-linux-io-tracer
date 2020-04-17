#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from core.test_run_utils import TestRun
from utils.iotrace import IotracePlugin


def install_afl():
    TestRun.LOGGER.info("Installing AFL")

    TestRun.executor.run_expect_success(
        f"cd {TestRun.plugins['iotrace'].working_dir}"
        f" && git clone http://github.com/google/AFL"
        f" && cd AFL && make && make install")


def is_afl_installed():
    TestRun.LOGGER.info("Checking for AFL")

    output = TestRun.executor.run(
        f"[ -d {TestRun.plugins['iotrace'].working_dir}/AFL ]")

    if output.exit_code != 0:
        return False

    output = TestRun.executor.run("afl-g++ --version")
    if output.exit_code != 0:
        return False

    return True


def create_patch_redirect_fuzz_to_file(file_path: str, patch_path: str):
    '''
    This creates a patch file which redirects fuzzed values from
    stdin to given absolute file_path. The file is saved on DUT
    as patch_path
    '''
    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    repo_path: str = f"{iotrace.working_dir}/slit-afl"

    # Create new patch file
    TestRun.executor.run_expect_success(f'cp {TestRun.plugins["iotrace"].working_dir}'
                                        '/standalone-linux-io-tracer/tests/security/fuzzy/redirect-fuzz.patch'
                                        f' {patch_path}')

    # Modify patch to use supplied fuzzed file path
    TestRun.executor.run_expect_success("sed -i 's@FUZZED_FILE_PATH@"+file_path+"@g'"
                                        f' {patch_path}')


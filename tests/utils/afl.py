#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from core.test_run_utils import TestRun


def install_afl():
    TestRun.LOGGER.info("Installing AFL")

    output = TestRun.executor.run(
        f"cd {TestRun.plugins['iotrace'].working_dir}"
        f" && git clone http://github.com/google/AFL"
        f" && cd AFL && make && make install")

    if output.exit_code != 0:
        raise Exception(
            "Installing AFL failed with nonzero status: "
            f"{output.stdout}\n{output.stderr}")


def is_afl_installed():
    TestRun.LOGGER.info("Checking for AFL")

    output = TestRun.executor.run("afl-g++")
    if output.exit_code != 0:
        return False

    return True

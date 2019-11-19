#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from log.logger import Log
from core.test_run_utils import TestRun


def install_iotrace():
    TestRun.plugins['iotrace'].installed = True

    TestRun.LOGGER.info("Copying standalone-linux-io-tracer repository to DUT")
    TestRun.executor.rsync(
        f"{TestRun.plugins['iotrace'].repo_dir}/",
        f"{TestRun.plugins['iotrace'].working_dir}/",
        delete=True,
        symlinks=True,
        exclude_list=['build'])

    TestRun.LOGGER.info("Installing dependencies")
    output = TestRun.executor.run(
        f"cd {TestRun.plugins['iotrace'].working_dir} && "
        "./setup_dependencies.sh")
    if output.exit_code != 0:
        TestRun.exception(
            f"Installing dependencies failed with nonzero status: {output.stdout}\n{output.stderr}")


    TestRun.LOGGER.info("Calling make all")
    output = TestRun.executor.run(
        f"cd {TestRun.plugins['iotrace'].working_dir} && "
        "make clean && make -j`nproc --all`")
    if output.exit_code != 0:
        TestRun.exception(
            f"Make command executed with nonzero status: {output.stdout}\n{output.stderr}")

    TestRun.LOGGER.info("Calling make install")
    output = TestRun.executor.run(
        f"cd {TestRun.plugins['iotrace'].working_dir} && "
        f"make install")
    if output.exit_code != 0:
        TestRun.exception(
            f"Error while installing iotrace: {output.stdout}\n{output.stderr}")

    TestRun.LOGGER.info("Checking if iotrace is properly installed.")
    output = TestRun.executor.run("iotrace -V")
    if output.exit_code != 0:
        TestRun.exception(
            f"'iotrace -V' command returned an error: {output.stdout}\n{output.stderr}")
    else:
        TestRun.LOGGER.debug(output.stdout)


def uninstall_iotrace():
    TestRun.LOGGER.info("Uninstalling iotrace")
    output = TestRun.executor.run("iotrace -V")
    if output.exit_code != 0:
        TestRun.exception("iotrace is not properly installed")
    else:
        TestRun.executor.run(
            f"cd {TestRun.plugins['iotrace'].working_dir} && "
            f"make uninstall")
        if output.exit_code != 0:
            TestRun.exception(
                f"There was an error during uninstall process: {output.stdout}\n{output.stderr}")

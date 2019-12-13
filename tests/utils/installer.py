#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from log.logger import Log
from core.test_run_utils import TestRun


def install_iotrace():
    uninstall_iotrace()

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
            "Installing dependencies failed with nonzero status: "
            f"{output.stdout}\n{output.stderr}")

    TestRun.LOGGER.info("Calling make all")
    output = TestRun.executor.run(
        f"cd {TestRun.plugins['iotrace'].working_dir} && "
        "make clean && make -j`nproc --all`")
    if output.exit_code != 0:
        TestRun.exception(
            "Make command executed with nonzero status: "
            f"{output.stdout}\n{output.stderr}")

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
            "'iotrace -V' command returned an error: "
            f"{output.stdout}\n{output.stderr}")
    else:
        TestRun.LOGGER.debug(output.stdout)

    TestRun.plugins['iotrace'].installed = True


def uninstall_iotrace():
    TestRun.LOGGER.info("Uninstalling iotrace")
    TestRun.executor.run(
        f"cd {TestRun.plugins['iotrace'].working_dir} && "
        "make uninstall")

def remove_module():
    output = TestRun.executor.run(f"rmmod iotrace")
    if output.exit_code != 0:
        TestRun.fail(
            f"rmmod failed with: {output.stdout}\n{output.stderr}")

def insert_module():
    TestRun.executor.run_expect_success(f"modprobe -r iotrace")
    output = TestRun.executor.run(f"modprobe iotrace")
    if output.exit_code != 0:
        TestRun.fail(
            f"modprobe failed with: {output.stdout}\n{output.stderr}")

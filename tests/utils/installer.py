#
# Copyright(c) 2019-2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
from datetime import timedelta
from shutil import which

from core.test_run_utils import TestRun


def install_iotrace_with_afl_support(patch_path: str):
    '''
        This copies the repository and applies diff patch from supplied path
        Then iotrace is built with AFL fuzzer support and installed locally
        in the build directory.

        The source code patch(es) are required to prepare iotrace for fuzzing,
        and may for example redirect fuzzed values from stdin to file.

        :param str patch_path: Path to patch file
    '''
    TestRun.LOGGER.info("Copying standalone-linux-io-tracer repository to DUT"
                        " for AFL fuzzy tests")

    iotrace = TestRun.plugins['iotrace']
    tracing_patch_path: str = "tests/security/fuzzy/immediate-tracing.patch"
    modprobe_disable_patch_path: str = "tests/security/fuzzy/disable_modprobe.patch"
    repo_path = f"{iotrace.working_dir}/slit-afl"

    TestRun.executor.rsync(
        f"{iotrace.repo_dir}/",
        f"{iotrace.working_dir}/slit-afl",
        delete=True,
        symlinks=True,
        exclude_list=['build'])

    # Copy neccessary files for patching
    TestRun.executor.run_expect_success(
        f'cp {iotrace.working_dir}/AFL/'
        f'experimental/argv_fuzzing/argv-fuzz-inl.h '
        f'{iotrace.working_dir}/slit-afl/source/userspace/')
    TestRun.executor.run_expect_success(
        f'cp {iotrace.working_dir}/tests/'
        f'security/fuzzy/afl-fuzzer-utils.h '
        f'{iotrace.working_dir}/slit-afl/source/userspace/')

    TestRun.LOGGER.info("Applying code patches")
    output = TestRun.executor.run(
        f'cd {repo_path} '
        f'&& patch -f -p0 -F4 <{tracing_patch_path} '
        f'&& patch -f -p0 -F4 <{modprobe_disable_patch_path} '
        f'&& patch -f -p0 -F4 <{patch_path}',
        timeout=timedelta(minutes=1))

    if output.exit_code != 0:
        raise Exception("Could not patch files with AFL patch, it's possible "
                        "that the source files changed too much to be able to "
                        "apply the patch. Apply patches manually. Failing test."
                        " Error: " + output.stderr)

    TestRun.LOGGER.info("Calling make install with AFL compiler")
    TestRun.executor.run_expect_success(
        f"cd {repo_path} && "
        f"make install PREFIX={repo_path}/rootfs CXX=afl-g++ -j`nproc --all`")


def install_iotrace():
    uninstall_iotrace()
    iotrace = TestRun.plugins['iotrace']

    TestRun.LOGGER.info("Copying standalone-linux-io-tracer repository to DUT")
    TestRun.executor.rsync(
        f"{iotrace.repo_dir}/",
        f"{iotrace.working_dir}/",
        delete=True,
        symlinks=True,
        exclude_list=['build'])

    TestRun.LOGGER.info("Installing dependencies")
    TestRun.executor.run_expect_success(
        f"cd {iotrace.working_dir} && "
        "./setup_dependencies.sh")

    TestRun.LOGGER.info("Calling make all")
    TestRun.executor.run_expect_success(
        f"cd {iotrace.working_dir} && "
        "make clean && make -j`nproc --all`")

    TestRun.LOGGER.info("Calling make install")
    TestRun.executor.run_expect_success(
        f"cd {iotrace.working_dir} && "
        f"make install")

    if not check_if_installed():
        raise Exception("iotrace wasn't properly installed")
    else:
        iotrace.installed = True


def check_if_installed():
    TestRun.LOGGER.info("Checking if iotrace is installed")
    output = TestRun.executor.run("iotrace -V")

    return output.exit_code == 0


def check_if_ubuntu():
    return which("apt") is not None


def uninstall_iotrace():
    TestRun.LOGGER.info("Uninstalling previous iotrace")

    if check_if_ubuntu():
        TestRun.executor.run("apt remove -y iotrace")
    else:
        TestRun.executor.run("rpm -e iotrace")
    TestRun.executor.run(
        f"cd {TestRun.plugins['iotrace'].working_dir} && "
        "make uninstall")


def remove_module():
    TestRun.executor.run_expect_success("modprobe -r iotrace")


def insert_module():
    TestRun.executor.run_expect_success(f"modprobe iotrace")

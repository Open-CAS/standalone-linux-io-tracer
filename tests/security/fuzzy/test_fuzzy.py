#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from core.test_run import TestRun
from utils.afl import is_afl_installed, install_afl
from utils.afl import create_patch_redirect_fuzz_to_file
from utils.installer import install_iotrace_with_afl_support
from utils.iotrace import IotracePlugin
from datetime import datetime

import time


# This test is to be run first to initialize fuzzing environment
def test_fuzz_args():
    fuzzing_time_seconds = 45 * 60
    iotrace: IotracePlugin = TestRun.plugins['iotrace']

    repo_path: str = f"{iotrace.working_dir}/slit-afl"
    disk = TestRun.dut.disks[0].system_path

    # Make sure AFL is installed
    if not is_afl_installed():
        install_afl()

    # Install iotrace locally with AFL support
    # Patch so that we redirect fuzzed stdin to argv
    install_iotrace_with_afl_support(
        repo_path + "/tests/security/fuzzy/redirect-fuzz-to-argv.patch")

    # Instruct the system to output coredumps as files instead of sending them
    # to a specific crash handler app
    TestRun.LOGGER.info('Setting up system for fuzzing')
    TestRun.executor.run_expect_success('echo core > /proc/sys/kernel/core_pattern')
    TestRun.executor.run_expect_success(f'cd {repo_path} && mkdir -p afl-i afl-o')

    # Add input seeds which shall be mutated - try list-traces, version, help,
    # start-tracing and remove-trace commands to guide the fuzzer.
    TestRun.executor.run_expect_success(f'cd {repo_path} && echo "-L"'
                                        f' > afl-i/case0')
    TestRun.executor.run_expect_success(f'cd {repo_path} && echo "-L -p k*"'
                                        f' > afl-i/case1')
    TestRun.executor.run_expect_success(f'cd {repo_path} && echo "-V"'
                                        f' > afl-i/case2')
    TestRun.executor.run_expect_success(f'cd {repo_path} && echo "-H"'
                                        f' > afl-i/case3')
    TestRun.executor.run_expect_success(f'cd {repo_path} && echo "-S -d {disk}"'
                                        f' > afl-i/case4')
    TestRun.executor.run_expect_success(f'cd {repo_path} && echo "-R -p k*"'
                                        f' > afl-i/case5')

    # Run script which will launch parallel fuzzers in separate 'screen'
    # windows in the background
    TestRun.LOGGER.info('Starting fuzzing argv. This should take ' +
                        str(fuzzing_time_seconds/60) + ' minutes')
    TestRun.executor.run(f'cd {repo_path} && ./tests/security/fuzzy/fuzz.sh '
                         'rootfs/bin/iotrace')

    # Wait for fuzzing completion and output logs
    output = wait_for_completion(fuzzing_time_seconds, repo_path)

    TestRun.LOGGER.info('Killing fuzzers')
    TestRun.executor.run(f'cd {repo_path} && ./tests/security/fuzzy/fuzz.sh clean')
    TestRun.executor.run_expect_success(f'rm -rf {repo_path}/afl-i')

    detect_crashes(output, "argv")


def test_fuzz_config():
    fuzzing_time_seconds = 10 * 60
    iotrace: IotracePlugin = TestRun.plugins['iotrace']

    repo_path: str = f"{iotrace.working_dir}/slit-afl"

    # Create patch file for redirecting fuzzed stdin to config file path
    new_patch_path: str = f'{iotrace.working_dir}/redirect_to_config.patch'
    create_patch_redirect_fuzz_to_file(f'{repo_path}/rootfs/etc/octf/octf.conf',
                                       new_patch_path)

    # Install iotrace locally with AFL support and redirect patch
    install_iotrace_with_afl_support(new_patch_path, ['modules/open-cas-telemetry-framework', 'modules/test-framework', 'source/kernel'])

    TestRun.executor.run_expect_success(f'cd {repo_path} && mkdir -p afl-i afl-o')
    # Use config as seed to be mutated
    TestRun.executor.run_expect_success(f'cp {repo_path}/rootfs/etc/octf/octf.conf'
                                        f' {repo_path}/afl-i/case0')

    TestRun.LOGGER.info('Starting fuzzing octf.conf. This should take ' +
                        str(fuzzing_time_seconds / 60) + ' minutes')

    TestRun.LOGGER.info("Trying 'list-traces' command")
    TestRun.executor.run(f'cd {repo_path} && ./tests/security/fuzzy/fuzz.sh '
                         '"rootfs/bin/iotrace -L" --one-job')
    output = wait_for_completion(fuzzing_time_seconds/2, repo_path)
    TestRun.executor.run(f'cd {repo_path} && ./tests/security/fuzzy/fuzz.sh clean')
    detect_crashes(output, "octf-config")

    TestRun.LOGGER.info("Trying 'get-trace-repository-path' command")
    TestRun.executor.run(f'cd {repo_path} && ./tests/security/fuzzy/fuzz.sh '
                         '"rootfs/bin/iotrace --get-trace-repository" --one-job')
    output = wait_for_completion(fuzzing_time_seconds/2, repo_path)
    TestRun.executor.run(f'cd {repo_path} && ./tests/security/fuzzy/fuzz.sh clean')
    TestRun.executor.run_expect_success(f'rm -rf {repo_path}/afl-i')
    detect_crashes(output, "octf-config")


def test_fuzz_trace_file():
    fuzzing_time_seconds = 20 * 60
    iotrace: IotracePlugin = TestRun.plugins['iotrace']

    repo_path: str = f"{iotrace.working_dir}/slit-afl"

    # Create trace files
    iotrace.start_tracing()
    iotrace.stop_tracing()
    trace_repo_path = IotracePlugin.get_trace_repository_path()
    trace_path = trace_repo_path + "/" + IotracePlugin.get_latest_trace_path()
    tracefile_path = f'{trace_path}/octf.trace.0'
    copied_tracefile_path = f'{repo_path}/rootfs/var/lib/octf/trace/' + \
                            f'{IotracePlugin.get_latest_trace_path()}' \
                            f'/octf.trace.0'

    # Create patch file for redirecting fuzzed stdin to trace file
    new_patch_path: str = f'{iotrace.working_dir}/redirect_to_tracefile.patch'
    create_patch_redirect_fuzz_to_file(f'{copied_tracefile_path}', new_patch_path)

    # Install iotrace locally with AFL support and redirect patch
    install_iotrace_with_afl_support(new_patch_path, ['modules/open-cas-telemetry-framework', 'modules/test-framework', 'source/kernel'])

    # Copy trace files to local instalation of iotrace
    TestRun.executor.run_expect_success(f'cp -r {trace_repo_path}/kernel '
                                        f'{repo_path}/rootfs/var/lib/octf/trace/')

    TestRun.executor.run_expect_success(f'cd {repo_path} && mkdir -p afl-i afl-o')
    # Add input seed which shall be mutated
    TestRun.executor.run_expect_success(f'cd {repo_path} && echo "0" > afl-i/case0')

    TestRun.LOGGER.info(f'Starting fuzzing {tracefile_path} This should take ' +
                        str(fuzzing_time_seconds / 60) + ' minutes')
    TestRun.executor.run(f'cd {repo_path} && ./tests/security/fuzzy/fuzz.sh '
                         '"rootfs/bin/iotrace --get-trace-statistics -p '
                         f'{IotracePlugin.get_latest_trace_path()}" --one-job')
    output = wait_for_completion(fuzzing_time_seconds, repo_path)
    TestRun.executor.run(f'cd {repo_path} && ./tests/security/fuzzy/fuzz.sh clean')
    TestRun.executor.run_expect_success(f'rm -rf {repo_path}/afl-i')
    detect_crashes(output, "trace-file")


def test_fuzz_summary_file():
    fuzzing_time_seconds = 10 * 60
    iotrace: IotracePlugin = TestRun.plugins['iotrace']

    repo_path: str = f"{iotrace.working_dir}/slit-afl"

    # Create trace files
    iotrace.start_tracing()
    iotrace.stop_tracing()
    trace_repo_path = IotracePlugin.get_trace_repository_path()
    trace_path = trace_repo_path + "/" + IotracePlugin.get_latest_trace_path()
    summary_path = f'{trace_path}/octf.summary'
    copied_summary_path = f'{repo_path}/rootfs/var/lib/octf/trace/' + \
                          f'{IotracePlugin.get_latest_trace_path()}' \
                          f'/octf.summary'

    # Create patch file for redirecting fuzzed stdin to trace file
    new_patch_path: str = f'{iotrace.working_dir}/redirect_to_summary.patch'
    create_patch_redirect_fuzz_to_file(f'{copied_summary_path}', new_patch_path)

    # Install iotrace locally with AFL support and redirect patch
    install_iotrace_with_afl_support(new_patch_path, ['modules/open-cas-telemetry-framework', 'modules/test-framework', 'source/kernel'])

    # Copy trace files to local installation of iotrace
    TestRun.executor.run_expect_success(f'cp -r {trace_repo_path}/kernel '
                                        f'{repo_path}/rootfs/var/lib/octf/trace/')

    TestRun.executor.run_expect_success(f'cd {repo_path} && mkdir -p afl-i afl-o')
    # Add input seed which shall be mutated
    TestRun.executor.run_expect_success(f'cd {repo_path} ' +
                                        '&& echo "{}" > afl-i/case0')

    TestRun.LOGGER.info(f'Starting fuzzing {summary_path} This should take ' +
                        str(fuzzing_time_seconds / 60) + ' minutes')
    TestRun.executor.run(f'cd {repo_path} && ./tests/security/fuzzy/fuzz.sh '
                         '"rootfs/bin/iotrace --get-trace-statistics -p '
                         f'{IotracePlugin.get_latest_trace_path()}" --one-job')
    output = wait_for_completion(fuzzing_time_seconds, repo_path)
    TestRun.executor.run(f'cd {repo_path} && ./tests/security/fuzzy/fuzz.sh clean')
    TestRun.executor.run_expect_success(f'rm -rf {repo_path}/afl-i')
    detect_crashes(output, "summary-file")


def test_fuzz_procfs():
    # For simplicity we start one non-instrumented iotrace instance
    # and fuzz proc files using afl with -n flag (dumb fuzzing)
    fuzzing_time_seconds = 15 * 60
    iotrace: IotracePlugin = TestRun.plugins['iotrace']

    repo_path: str = f"{iotrace.working_dir}/slit-afl"

    # Reset dmesg so we don't accidentaly grep old kernel panics
    TestRun.executor.run_expect_success('dmesg -c')

    # Procfs files and their initial seed
    fuzzed_files = {"/proc/iotrace/add_device": "/dev/sdb",
                    "/proc/iotrace/remove_device": "/dev/sdb",
                    "/proc/iotrace/size": "1024"}

    # Start tracing, we start it using procfs interface by putting traced
    # device path, buffer size, and mmaping (using fio with mmap engine)
    # consumer header procfile. This is because procfiles may have a limit
    # on applications which mmap them, and iotrace -S may block other mmaps
    disk = TestRun.dut.disks[0].system_path
    TestRun.executor.run_expect_success(f'modprobe iotrace')
    TestRun.executor.run_expect_fail(f'echo {disk} > /proc/iotrace/add_device')
    TestRun.executor.run_expect_success(f'echo 1024 > /proc/iotrace/size')
    fio_pid = TestRun.executor.run_in_background(f'fio --name=job --direct=0 --ioengine=mmap '
                                                 f'--filename=/proc/iotrace/consumer_hdr.0 '
                                                 f"--buffer_pattern=\\'/dev/urandom\\' "
                                                 f'--time_based --runtime=30s --bs=4k '
                                                 f'--iodepth=128 --rw=randwrite')
    time.sleep(5)
    TestRun.executor.run_expect_success(f'echo {disk} > /proc/iotrace/add_device')

    # Also start some workload on traced device to generate trace traffic
    workload_pid = TestRun.executor.run_in_background(f'fio --direct=1'
                                                 f'--filename={disk} '
                                                 f'--time_based --runtime=24h --bs=4k '
                                                 f'--iodepth=1 --rw=randwrite --name=job2 --size=4k')

    for procfile, seed in fuzzed_files.items():
        # Create patch file for redirecting fuzzed stdin to proc file
        new_patch_path: str = f'{iotrace.working_dir}/redirect_to_procfile.patch'
        create_patch_redirect_fuzz_to_file(f'{procfile}', new_patch_path)

        # Install iotrace locally with AFL support and redirect patch
        install_iotrace_with_afl_support(new_patch_path,
                                         ['modules/open-cas-telemetry-framework', 'modules/test-framework', 'source/kernel'])

        TestRun.executor.run_expect_success(
            f'cd {repo_path} && mkdir -p afl-i afl-o')
        # Add input seed which shall be mutated
        TestRun.executor.run_expect_success(f'cd {repo_path} && echo {seed} > afl-i/case0')

        TestRun.LOGGER.info(f'Starting fuzzing {procfile} This should take ' +
                            str(fuzzing_time_seconds / 60 / len(fuzzed_files))
                            + ' minutes')

        # May need to set CPU to performance mode beforehand:
        # cd /sys/devices/system/cpu
        # echo performance | tee cpu*/cpufreq/scaling_governor
        TestRun.executor.run_in_background(f'cd {repo_path} && screen -S master -d -m &&'
                                           f'screen -S master -X stuff "afl-fuzz -n -i '
                                           f'afl-i -o afl-o {repo_path}/rootfs/bin/iotrace -H\n"')
        elapsed = 0
        start_time = time.time()
        while elapsed < fuzzing_time_seconds / len(fuzzed_files):
            output = TestRun.executor.run(f'dmesg | grep -A 40 -B 10 "Call Trace"')
            if output.exit_code == 0:
                save_fuzzing_output(f'{repo_path}/afl-o', f'procfs-{procfile.split("/")[-1]}')
                TestRun.fail(f'Kernel BUGs during fuzzing {procfile} were found:\n'
                             f'{output.stdout}')

            time.sleep(5)
            current_time = time.time()
            elapsed = current_time - start_time

        TestRun.executor.run_expect_success('killall afl-fuzz && screen -X -S master kill')

    # Stop tracing
    TestRun.executor.run(f'kill {fio_pid}')
    TestRun.executor.run_expect_success(f'kill {workload_pid}')
    TestRun.executor.run_expect_success(f'rm -rf {repo_path}/afl-i')


# TODO (trybicki): use as AFL plugin which handles start, stop and logging
def wait_for_completion(fuzzing_time_seconds: int, repo_path: str):
    elapsed = 0
    start_time = time.time()
    while elapsed < fuzzing_time_seconds:
        output = TestRun.executor.run(
            f'cd {repo_path} && sleep 1 && afl-whatsup afl-o')
        time.sleep(10)
        current_time = time.time()
        elapsed = current_time - start_time

    # Return last output
    return output


def detect_crashes(fuzz_output, label: str):
    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    repo_path: str = f"{iotrace.working_dir}/slit-afl"

    if 'Crashes found : 0' not in fuzz_output.stdout\
            and fuzz_output.exit_code != 1:
        save_fuzzing_output(f'{repo_path}/afl-o', label)
        TestRun.fail('Crashes during fuzzing were found. Please find the'
                     f' inputs which cause it in {iotrace.working_dir}/'
                     f'fuzzy-crashes/ directory')


# Save fuzzing output dir to working_dir/fuzzy_crashes to
# avoid being overwritten by next fuzzy test
def save_fuzzing_output(output_path: str, label: str):
    now = datetime.now()
    date: str = now.strftime("%H:%M:%S")

    iotrace: IotracePlugin = TestRun.plugins['iotrace']
    TestRun.executor.run_expect_success(f'mkdir -p {iotrace.working_dir}'
                                        f'/fuzzy-crashes/{label}/{date}')
    TestRun.executor.run_expect_success(f'cp -r {output_path} '
                                        f'{iotrace.working_dir}/fuzzy-crashes/'
                                        f'{label}/{date}/')

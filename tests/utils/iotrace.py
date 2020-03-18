#
# Copyright(c) 2019-2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import json
import os
import re
import time
import yaml
from datetime import timedelta

from api.iotrace_lat_hist_parser import LatencyHistograms
from core.test_run_utils import TestRun
from test_tools.fs_utils import check_if_directory_exists, create_directory
from test_utils.output import CmdException
from test_utils.size import Unit, Size
from utils.git import get_current_commit_hash, get_current_commit_message
from utils.git import get_current_octf_hash
from log.logger import create_log, Log

NOT_WHITESPACE = re.compile(r'[^\s]')


class IotracePlugin:
    def __init__(self):
        # Test controller's repo, copied to DUT
        self.repo_dir = os.path.join(os.path.dirname(__file__), "../../")

        # DUT's make/install work directory
        self.working_dir = "~"

        # Flag indicating if install iotrace
        self.reinstall = False

        self.pid = None

    def runtest_setup(self, item) -> object:
        try:
            with open(item.config.getoption('--dut-config')) as cfg:
                dut_config = yaml.safe_load(cfg)
        except Exception:
            raise Exception("You need to specify DUT config. "
                            "See the example_dut_config.py file.")

        try:
            TestRun.prepare(item, dut_config)

            test_name = item.name.split('[')[0]
            TestRun.LOGGER = create_log(item.config.getoption('--log-path'), test_name)
            TestRun.setup()

            self.working_dir = dut_config['working_dir']
            self.reinstall = item.config.getoption('--force-reinstall')
        except Exception as ex:
            raise Exception(f"Exception occurred during test setup:\n"
                            f"{str(ex)}\n{traceback.format_exc()}")

        TestRun.LOGGER.info(f"DUT info: {TestRun.dut}")

        # Prepare DUT for test
        TestRun.LOGGER.add_build_info(f'Commit hash:')
        TestRun.LOGGER.add_build_info(f"{get_current_commit_hash()}")
        TestRun.LOGGER.add_build_info(f'Commit message:')
        TestRun.LOGGER.add_build_info(f'{get_current_commit_message()}')
        TestRun.LOGGER.add_build_info(f'OCTF commit hash:')
        TestRun.LOGGER.add_build_info(f'{get_current_octf_hash()}')

        TestRun.LOGGER.info(f"DUT info: {TestRun.dut}")
        TestRun.LOGGER.write_to_command_log("Test body")
        TestRun.LOGGER.start_group("Test body")

    @staticmethod
    def runtest_teardown():
        TestRun.LOGGER.end_all_groups()

        with TestRun.LOGGER.step("Cleanup after test"):
            try:
                if TestRun.executor.is_active():
                    TestRun.executor.wait_for_connection()
            except Exception:
                TestRun.LOGGER.warning("Exception occured during platform cleanup.")

        TestRun.LOGGER.end()
        if TestRun.executor:
            TestRun.LOGGER.get_additional_logs()
        Log.destroy()
        TestRun.teardown()

    def start_tracing(self,
                      bdevs: list = [],
                      buffer: Size = None,
                      trace_file_size: Size = None,
                      timeout: timedelta = None,
                      label: str = None,
                      shortcut: bool = False):
        """
        Start tracing given block devices. Trace all available if none given.

        :param bdevs: Block devices to trace, can be empty
        (for all available)
        :param buffer: Size of the internal trace buffer in MiB
        :param trace_file_size: Max size of trace file in MiB
        :param timeout: Max trace duration time in seconds
        :param label: User defined custom label
        :param shortcut: Use shorter command
        :type bdevs: list of strings
        :type buffer: Size
        :type trace_file_size: Size
        :type timeout: timedelta
        :type label: str
        :type shortcut: bool
        """

        if len(bdevs) == 0:
            disks = TestRun.dut.disks
            for disk in disks:
                bdevs.append(disk.system_path)

        buffer_range = range(1, 1025)
        trace_file_size_range = range(1, 100000001)
        timeout_range = range(1, 4294967296)

        command = 'iotrace' + (' -S' if shortcut else ' --start-tracing')
        command += (' -d ' if shortcut else ' --devices ') + ','.join(bdevs)

        if buffer is not None:
            if not int(buffer.get_value(Unit.MebiByte)) in buffer_range:
                raise CmdException(f"Given buffer is out of range {buffer_range}.")
            command += ' -b ' if shortcut else ' --buffer '
            command += f'{int(buffer.get_value(Unit.MebiByte))}'

        if trace_file_size is not None:
            if not int(trace_file_size.get_value(Unit.MebiByte)) in trace_file_size_range:
                raise CmdException(f"Given size is out of range {trace_file_size_range}.")
            command += ' -s ' if shortcut else ' --size '
            command += f'{int(trace_file_size.get_value(Unit.MebiByte))}'

        if timeout is not None:
            if not int(timeout.total_seconds()) in timeout_range:
                raise CmdException(f"Given time is out of range {timeout_range}.")
            command += ' -t ' if shortcut else ' --time '
            command += f'{int(timeout.total_seconds())}'

        if label is not None:
            command += ' -l ' if shortcut else ' --label ' + f'{label}'

        self.pid = str(TestRun.executor.run_in_background(command))
        TestRun.LOGGER.info("Started tracing of: " + ','.join(bdevs))

    @staticmethod
    def get_trace_repository_path(shortcut: bool = False) -> list:
        """
        Get the path to trace repository from iotrace

        :param shortcut: Use shorter command
        :type shortcut: bool
        :return: list of dictionaries with trace repository path
        :raises Exception: when cannot find the path
        """
        command = 'iotrace' + (' -C' if shortcut else ' --trace-config')
        command += ' -G ' if shortcut else ' --get-trace-repository-path '

        return parse_json(TestRun.executor.run_expect_success(command).stdout)[0]["path"]

    def check_if_tracing_active(self) -> bool:
        """
        Check if tracing is active

        :return: True if correct iotrace process found, False otherwise
        :rtype: bool
        """
        output = TestRun.executor.run('pgrep iotrace')

        if output.stdout == "":
            TestRun.LOGGER.info("Iotrace processes not found.")
            return False

        elif self.pid != output.stdout:
            TestRun.LOGGER.info(f"Found other iotrace process with PID {output.stdout}")
            return False

        else:
            return True

    def stop_tracing(self) -> bool:
        """
        Stop tracing.

        :return: True if tracing was stopped, False no tracing was active
        :rtype: bool
        """
        TestRun.LOGGER.info("Stopping tracing")

        # Send sigints
        kill_attempts = 30
        attempt = 0

        while self.check_if_tracing_active() and attempt < kill_attempts:
            TestRun.LOGGER.info("Sending sigint no. " + str(attempt + 1))
            attempt += 1
            TestRun.executor.run(f'kill -2 {self.pid}')
            time.sleep(2)

        if self.check_if_tracing_active():
            TestRun.LOGGER.error("Could not kill iotrace")
            return False

        return True

    def kill_tracing(self) -> bool:
        """
        Kill tracing.

        :return: True if tracing was killed
        :rtype: bool
        :raises Exception: if failed to kill iotrace process
        """
        TestRun.LOGGER.info("Killing tracing")

        # Send -9
        kill_attempts = 30
        attempt = 0

        while self.check_if_tracing_active() and attempt < kill_attempts:
            TestRun.LOGGER.info("Sending -9 no. " + str(attempt + 1))
            attempt += 1
            TestRun.executor.run(f'kill -9 {self.pid}')
            time.sleep(2)

        if self.check_if_tracing_active():
            raise CmdException("Could not kill iotrace")

        return True

    @staticmethod
    def get_traces_list(prefix: str = None, shortcut: bool = False) -> list:
        """
        Returns list of traces

        :param prefix: Trace path or trace path's prefix when ended with '*'
        :param shortcut: Use shorter command
        :type prefix: str
        :type shortcut: bool
        :return: list of traces
        """
        command = 'iotrace' + (' -M' if shortcut else ' --trace-management')
        command += ' -L ' if shortcut else ' --list-traces '

        if prefix is not None:
            command += (' -p ' if shortcut else ' --prefix ') + f'{prefix}'
        output = TestRun.executor.run_expect_success(command)

        return parse_json(output.stdout)

    @staticmethod
    def get_trace_count(prefix: str = None, shortcut: bool = False) -> int:
        """
        Returns number of traces

        :param prefix: Trace path or trace path's prefix when ended with '*'
        :param shortcut: Use shorter command
        :type prefix: str
        :type shortcut: bool
        :return: number of traces
        """
        paths_parsed = IotracePlugin.get_traces_list(prefix, shortcut)
        if len(paths_parsed):
            return len(paths_parsed[0]['trace'])
        return 0

    @staticmethod
    def get_latest_trace_path(prefix: str = None, shortcut: bool = False) -> list:
        """
        Returns trace path of the most recent trace

        :param prefix: Trace path or trace path's prefix when ended with '*'
        :param shortcut: Use shorter command
        :type prefix: str
        :type shortcut: bool
        :return: trace path
        """
        paths_parsed = IotracePlugin.get_traces_list(prefix, shortcut)

        # Return the last element of trace list
        if len(paths_parsed) and len(paths_parsed[0]['trace']):
            return paths_parsed[0]['trace'][-1]['tracePath']
        else:
            return ""

    @staticmethod
    def get_trace_summary(trace_path: str, shortcut: bool = False) -> list:
        """
        Get trace summary of given trace path

        :param trace_path: trace path
        :param shortcut: Use shorter command
        :type trace_path: str
        :type shortcut: bool
        :return: Summary of trace
        :raises Exception: if summary is invalid
        """
        command = 'iotrace' + (' -M' if shortcut else ' --trace-management')
        command += ' -G' if shortcut else ' --get-trace-summary'
        command += (' -p ' if shortcut else ' --path ') + f'{trace_path}'

        output = TestRun.executor.run(command)
        if output.stdout == "":
            raise CmdException("Invalid summary", output)

        return parse_json(output.stdout)[0]

    @staticmethod
    def get_lba_histogram(trace_path: str,
                          bucket_size: Size = Size(0, Unit.Byte),
                          subrange_start: int = 0,
                          subrange_end: int = 0,
                          shortcut: bool = False) -> list:
        """
        Get lba histogram of given trace path

        :param trace_path: trace path
        :param bucket_size: bucket size
        :param subrange_start: subrange start
        :param subrange_end: subrange end
        :param shortcut: Use shorter command
        :type trace_path: str
        :type bucket_size: Size
        :type subrange_start: int
        :type subrange_end: int
        :type shortcut: bool
        :return: LBA histogram
        :raises Exception: if iotrace command or histogram is invalid
        """
        bucket_size_range = range(1, 4294967296)
        subrange_range = range(1, 9223372036854775808)
        if subrange_start and subrange_end:
            if subrange_start > subrange_end:
                subrange_start, subrange_end = subrange_end, subrange_start

        command = 'iotrace' + (' -P' if shortcut else ' --trace-parser')
        command += ' -B' if shortcut else ' --lba-histogram'
        command += (' -p ' if shortcut else ' --path ') + f'{trace_path}'

        if bucket_size is not None:
            if int(bucket_size.get_value(Unit.Byte)) not in bucket_size_range:
                raise CmdException(f"Given size is out of range {bucket_size_range}.")
            command += ' -b ' if shortcut else ' --bucket-size '
            command += f'{int(bucket_size.get_value(Unit.Byte))}'

        if subrange_start is not None:
            if subrange_start not in subrange_range:
                raise CmdException(f"Given start position is out of range {subrange_range}.")
            command += ' -s ' if shortcut else ' --subrange-start '
            command += f'{subrange_start}'

        if subrange_end is not None:
            if subrange_end not in subrange_range:
                raise CmdException(f"Given end position is out of range {subrange_range}.")
            command += ' -e ' if shortcut else ' --subrange-end '
            command += f'{subrange_end}'
        command += (' -f ' if shortcut else ' --format ') + 'json'

        output = TestRun.executor.run(command)
        if output.stdout == "":
            raise CmdException("Invalid histogram", output)

        return parse_json(output.stdout)

    @staticmethod
    def get_latency_histograms(trace_path: str, shortcut: bool = False) -> LatencyHistograms:
        """
        Get latency histogram of given trace path

        :param trace_path: trace path
        :param shortcut: Use shorter command
        :type trace_path: str
        :type shortcut: bool
        :return: latency histogram
        :rtype: LatencyHistograms
        :raises Exception: if histogram is invalid
        """
        command = 'iotrace' + (' -P' if shortcut else ' --trace-parser')
        command += ' -L' if shortcut else ' --latency-histogram'
        command += (' -p ' if shortcut else ' --path ') + f'{trace_path}'
        command += (' -f ' if shortcut else ' --format ') + 'json'

        output = TestRun.executor.run(command)
        if output.stdout == "":
            raise CmdException("Invalid histogram", output)

        return LatencyHistograms(parse_json(output.stdout)[0]['histogram'][0])

    @staticmethod
    def get_fs_statistics(trace_path: str, shortcut: bool = False) -> list:
        """
        Get filesystem statistics of given trace path

        :param trace_path: trace path
        :param shortcut: Use shorter command
        :type trace_path: str
        :type shortcut: bool
        :return: Filesystem statistics
        :raises Exception: if statistics are invalid
        """
        command = 'iotrace' + (' -P' if shortcut else ' --trace-parser')
        command += ' -F' if shortcut else ' --fs-statistics'
        command += (' -p ' if shortcut else ' --path ') + f'{trace_path}'
        command += (' -f ' if shortcut else ' --format ') + 'json'

        output = TestRun.executor.run(command)
        if output.stdout == "":
            raise CmdException("Invalid filesystem statistics", output)

        return parse_json(output.stdout)

    @staticmethod
    def get_trace_events(trace_path: str, raw: bool = False, shortcut: bool = False) -> list:
        """
        Get all trace events of given trace path

        :param trace_path: trace path
        :param raw: output without processing
        :param shortcut: Use shorter command
        :type trace_path: str
        :type raw: bool
        :type shortcut: bool
        :return: trace events
        :raises Exception: if traces are invalid
        """
        command = 'iotrace' + (' -P' if shortcut else ' --trace-parser')
        command += ' -P' if shortcut else ' --io'
        command += (' -p ' if shortcut else ' --path ') + f'{trace_path}'
        command += (' -f ' if shortcut else ' --format ') + 'json'

        if raw:
            command += ' -r ' if shortcut else ' --raw '

        output = TestRun.executor.run(command)
        if output.stdout == "":
            raise CmdException("Invalid traces", output)

        return parse_json(output.stdout)

    @staticmethod
    def get_trace_statistics(trace_path: str, dev_path: str = "", shortcut: bool = False) -> list:
        """
        Get statistics of particular trace

        :param trace_path: trace path
        :param dev_path: path of device which should be retrieved
        :param shortcut: Use shorter command
        :type trace_path: str
        :type shortcut: bool
        :return: If @dev_path specified - trace statistics, list of trace stats otherwise
        :raises Exception: if statistics are invalid
        """
        command = 'iotrace' + (' -P' if shortcut else ' --trace-parser')
        command += ' -S' if shortcut else ' --statistics'
        command += (' -p ' if shortcut else ' --path ') + f'{trace_path}'
        command += (' -f ' if shortcut else ' --format ') + 'json'

        output = TestRun.executor.run(command)
        if output.stdout == "":
            raise CmdException("Invalid IO statistics", output)

        if dev_path == "":
            return parse_json(output.stdout)[0]['statistics']

        expected_device_name  = dev_path.split('/dev/')[1].replace("/","")

        for trace_stat in parse_json(output.stdout)[0]['statistics']:
            traced_device = trace_stat["desc"]["device"]["name"]
            if  traced_device == expected_device_name:
                return trace_stat

        raise CmdException(f"No trace stats for device {dev_path}", output)


    @staticmethod
    def remove_traces(prefix: str, force: bool = False, shortcut: bool = False):
        """
        Removes record of the most recent trace

        :param prefix: Trace path or trace path's prefix when ended with '*'
        :param force: force removal
        :param shortcut: Use shorter command
        :type prefix: str
        :type force: bool
        :type shortcut: bool
        :raises Exception: if removal is invalid
        """
        command = 'iotrace' + (' -M' if shortcut else ' --trace-management')
        command += ' -R' if shortcut else ' --remove-traces'
        command += (' -p ' if shortcut else ' --prefix ') + f'{prefix}'

        if force:
            command += ' -f ' if shortcut else ' --force '

        output = TestRun.executor.run(command)
        if output.exit_code == 0:
            return parse_json(output.stdout)
        error_output = parse_json(output.stderr)[0]['trace']
        if error_output == "No traces removed.":
            return ""
        return error_output

    @staticmethod
    def set_trace_repository_path(trace_path: str, shortcut: bool = False):
        """
        :param trace_path: trace path
        :param shortcut: Use shorter command
        :type trace_path: str
        :type shortcut: bool
        :raises Exception: if setting path fails
        """
        if not check_if_directory_exists(trace_path):
            create_directory(trace_path)

        command = 'iotrace' + (' -C' if shortcut else ' --trace-config')
        command += ' -S ' if shortcut else ' --set-trace-repository-path '
        command += (' -p ' if shortcut else ' --path ') + f'{trace_path}'

        output = TestRun.executor.run(command)
        error_output = parse_json(output.stderr)[0]["trace"]
        if error_output == "No access to trace directory":
            raise CmdException("Invalid setting of the trace repository path", output)

    @staticmethod
    def help(shortcut: bool = False) -> str:
        """
        :param shortcut: Use shorter command
        :type shortcut: bool
        :return: io-tracer help
        :raises Exception: if command fails
        """
        return TestRun.executor.run_expect_success(
            'iotrace' + (' -H ' if shortcut else ' --help ')).stdout

    @staticmethod
    def version(shortcut: bool = False) -> dict:
        """
        :param shortcut: Use shorter command
        :type shortcut: bool
        :return: io-tracer version
        :rtype: dict {'iotrace': str, 'OCTF': str}
        :raises Exception: if command fails
        """
        output = parse_json(TestRun.executor.run_expect_success(
            'iotrace' + (' -V ' if shortcut else ' --version ')).stdout)

        version = {output[0]["system"]: output[0]["trace"],
                   output[1]["system"]: output[1]["trace"]}

        return version


def parse_json(output: str):
    """
    Parse a string with json messages to a list of python dictionaries

    :param output: JSON output to be parsed into python dicts
    :return: List of dictionaries with fields corresponding
    to json messages fields
    :rtype: List of dicts
    """
    msgs_list = []
    for obj in __decode_json_stream(output):
        msgs_list.append(obj)
    return msgs_list


# Get json objects from a stream of concatenated json messages
#
# raw_decode stops once it has a valid object and returns the last position
# where wasn't part of the parsed object. It's not documented, but you can
# pass this position back to raw_decode and it start parsing again from
# that position. Unfortunately, the Python json module doesn't accept
# strings that have prefixing whitespace. So we need to search to find
# the first none-whitespace part of your document.


def __decode_json_stream(document, pos=0, decoder=json.JSONDecoder()):
    while True:
        # Create json stream without whitespace
        match = NOT_WHITESPACE.search(document, pos)

        if not match:
            # No more data
            return
        pos = match.start()

        try:
            obj, pos = decoder.raw_decode(document, pos)
        except json.JSONDecodeError:
            raise Exception("Invalid json formatting")

        yield obj

#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import json
import re
import time
from datetime import timedelta
from core.test_run_utils import TestRun
from test_utils.singleton import Singleton

NOT_WHITESPACE = re.compile(r'[^\s]')

# Singleton class to provide test-session wide scope
class IotracePlugin(metaclass=Singleton):
    def __init__(self, repo_dir, working_dir):
        self.repo_dir = repo_dir  # Test controller's repo, copied to DUT
        self.working_dir = working_dir  # DUT's make/install work directory
        self.installed = False  # Was iotrace installed already

    def start_tracing(self, bdevs=[]):
        '''
        Start tracing given block devices. Trace all available if none given.

        :param dev_list: Block devices to trace, can be empty (for all available)
        :type dev_list: list of strings

        :raises Exception: if iotrace binary exited early
        '''

        if len(bdevs) == 0:
            disks = TestRun.dut.disks
            for disk in disks:
                bdevs.append(disk.system_path)

        TestRun.executor.run_in_background('iotrace -S -d ' + ','.join(bdevs))
        TestRun.LOGGER.info("Started tracing of: " + ','.join(bdevs))


    def check_if_tracing_active(self):
        '''
        Check if tracing is active

        :return: True if iotrace process found, False otherwise
        :rtype: Bool
        '''
        output = TestRun.executor.run('pgrep iotrace')
        if output.stdout == "":
            return False
        else:
            return True

    def stop_tracing(self):
        '''
        Stop tracing.

        :return: True if tracing was stopped, False no tracing was active
        :rtype: Bool
        :raises Exception: if could not stop tracing which is active
        '''
        TestRun.LOGGER.info("Stopping tracing")
        pid = TestRun.executor.run('pgrep iotrace')
        if pid.stdout == "":
            return False

        # Send sigints
        kill_attempts = 30
        attempt = 0
        while pid.stdout != "" and attempt < kill_attempts:
            TestRun.LOGGER.info("Sending sigint no. " + str(attempt))
            attempt += 1
            TestRun.executor.run(f'kill -s SIGINT {pid.stdout}')
            time.sleep(2)
            pid = TestRun.executor.run('pgrep iotrace')

        if pid.stdout != "":
            raise Exception("Could not kill iotrace")

        return True

    def get_latest_trace_path(self):
        '''
        Returns trace path of most recent trace

        :return: trace path
        :rtype: string
        '''
        output = TestRun.executor.run('iotrace -L')
        paths_parsed = self.parse_json(output.stdout)

        # Sort trace paths
        def get_sort_key(element):
            return element['tracePath']

        paths_parsed[0]['trace'].sort(key=get_sort_key)

        # Return the last element of trace list
        if len(paths_parsed):
            return paths_parsed[0]['trace'][-1]['tracePath']
        else:
            return ""

    def get_trace_summary(self, trace_path):
        '''
        Get trace summary of given trace path

        :param str trace_path: trace path
        :return: Summary of trace
        :rtype: string
        :raises Exception: if summary is invalid
        '''
        output = TestRun.executor.run(f'iotrace --get-trace-summary -p {trace_path}')
        if (output.stdout == ""):
            raise Exception("Invalid summary")

        return output.stdout

    def parse_json(self, output):
        '''
        Parse a string with json messages to a list of python dictionaries

        :param str output: JSON output to be parsed into python dicts

        :return: List of dictionaries with fields corresponding to json messages fields
        :rtype: List of dicts
        '''
        msgs_list = []
        for obj in self.__decode_json_stream(output):
            msgs_list.append(obj)
        return msgs_list

    # Get json objects from a stream of concatenated json messages
    #
    # raw_decode stops once it has a valid object and returns the last position
    # where wasn't part of the parsed object. It's not documented, but you can
    # pass this position back to raw_decode and it start parsing again from that
    # position. Unfortunately, the Python json module doesn't accept strings
    # that have prefixing whitespace. So we need to search to find the first
    # none-whitespace part of your document.
    def __decode_json_stream(self, document, pos=0, decoder=json.JSONDecoder()):
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

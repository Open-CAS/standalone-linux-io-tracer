#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import json
import re
import pprint
from log.logger import Log
from core.test_run_utils import TestRun
from test_utils.singleton import Singleton


NOT_WHITESPACE = re.compile(r'[^\s]')


# Singleton class to provide test-session wide scope
class IotracePlugin(metaclass=Singleton):
    def __init__(self, repo_dir, working_dir):
        self.repo_dir = repo_dir        # Test controller's repo, copied to DUT
        self.working_dir = working_dir  # DUT's make/install work directory
        self.installed = False          # Was iotrace installed already


    def parse_output(self, output):
        msgs_list = []
        for obj in self.__decode_json_stream(output):
            msgs_list.append(obj)
        return msgs_list


    # Get json objects from a stream of concatenated json messages
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
                    raise Exception("Ivalid json formatting")

            yield obj

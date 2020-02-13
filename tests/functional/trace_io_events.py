#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from core.test_run import TestRun
from test_tools.dd import Dd
from test_utils.size import Size, Unit
import time


def test_io_events():
    TestRun.LOGGER.info("Testing io events during tracing")
    iotrace = TestRun.plugins['iotrace']

    for disk in TestRun.dut.disks:
        with TestRun.step("Start tracing"):
            iotrace.start_tracing([disk.system_path])
            time.sleep(5)
        with TestRun.step("Send write command"):
            write_length = Size(17, disk.block_size)
            dd = (
                Dd().input("/dev/urandom").output(disk.system_path).count(1).
                    block_size(write_length).oflag('direct,sync').seek(2))
            dd.run()
        with TestRun.step("Send read command"):
            read_length = Size(19, disk.block_size)
            dd = (Dd().input(disk.system_path).output("/dev/null").count(
                1).block_size(read_length).iflag('direct,sync').skip(2))
            dd.run()
        with TestRun.step("Send discard command"):
            discard_length = Size(21, disk.block_size)
            TestRun.executor.\
                run_expect_success(f"blkdiscard -o {2 * int(discard_length.get_value())}"
                                   f" -l {int(discard_length.get_value())} {disk.system_path}")
        with TestRun.step("Stop tracing"):
            iotrace.stop_tracing()
        with TestRun.step("Verify trace correctness"):
            trace_path = iotrace.get_latest_trace_path()
            events = iotrace.get_trace_events(trace_path)
            events_parsed = iotrace.parse_json(events)
            result = any(
                'io' in event and
                'operation' in event['io'] and
                event['io']['operation'] == 'Write' and
                # LBA 0 events don't have a lba field, so skip them
                'lba' in event['io'] and
                int(event['io']['lba']) == int(2 * write_length.get_value() / 512) and
                int(event['io']['len']) == int(write_length.get_value() / 512) and
                f"/dev/{event['device']['name']}" == disk.system_path for
                event in events_parsed)
            if not result:
                raise Exception("Could not find write event")
            result = any(
                'io' in event and
                # Read events don't have an operation field
                'operation' not in event['io'] and
                'lba' in event['io'] and
                int(event['io']['lba']) == int(2 * read_length.get_value() / 512) and
                int(event['io']['len']) == int(read_length.get_value() / 512) and
                f"/dev/{event['device']['name']}" == disk.system_path for
                event in events_parsed)
            if not result:
                raise Exception("Could not find read event")
            result = any(
                'io' in event and
                'operation' in event['io'] and
                event['io']['operation'] == 'Discard' and
                'lba' in event['io'] and
                int(event['io']['lba']) == int(2 * discard_length.get_value() / 512) and
                int(event['io']['len']) == int(discard_length.get_value() / 512) and
                f"/dev/{event['device']['name']}" == disk.system_path for
                event in events_parsed)
            if not result:
                raise Exception("Could not find discard event")

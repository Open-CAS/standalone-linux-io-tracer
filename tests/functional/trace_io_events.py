#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from core.test_run import TestRun
from test_tools.dd import Dd
from test_utils.size import Size, Unit
import time
from random import randrange
from math import floor


def round_down(val, multiple):
    return floor(val / multiple) * multiple


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


def test_lba_histogram():
    TestRun.LOGGER.info("Testing lba histogram")
    iotrace = TestRun.plugins['iotrace']
    number_buckets = 32
    bucket_size = 256
    start_lba = 10240
    end_lba = start_lba + number_buckets * bucket_size
    for disk in TestRun.dut.disks:
        io_len = Size(1, disk.block_size)
        with TestRun.step("Start tracing"):
            iotrace.start_tracing([disk.system_path])
            time.sleep(5)
        with TestRun.step("Send write commands"):
            for bucket_index in range(number_buckets):
                for i in range(bucket_index + 1):
                    seek = (start_lba + bucket_index * bucket_size + randrange(bucket_size)) * 512 / disk.block_size.get_value()
                    dd = (Dd().input("/dev/urandom").output(disk.system_path).count(
                        1).block_size(io_len).oflag('direct,sync').seek(int(seek)))
                    dd.run()
        with TestRun.step("Send read commands"):
            for bucket_index in range(number_buckets):
                for i in range(bucket_index + 1):
                    seek = (start_lba + bucket_index * bucket_size + randrange(bucket_size)) * 512 / disk.block_size.get_value()
                    dd = (
                        Dd().input(disk.system_path).output("/dev/null").count(
                            1).block_size(io_len).iflag(
                            'direct,sync').skip(int(seek)))
                    dd.run()
        with TestRun.step("Send discard commands"):
            for bucket_index in range(number_buckets):
                for i in range(bucket_index + 1):
                    seek = start_lba + bucket_index * bucket_size + randrange(bucket_size)
                    seek = round_down(seek * 512, disk.block_size.get_value())
                    TestRun.executor.run_expect_success(f"blkdiscard -o {int(seek)} "
                                   f" -l {disk.block_size.get_value()} {disk.system_path}")
        with TestRun.step("Stop tracing"):
            iotrace.stop_tracing()
        with TestRun.step("Verify histogram correctness"):
            trace_path = iotrace.get_latest_trace_path()
            histogram = iotrace.get_lba_histogram(trace_path, bucket_size, start_lba, end_lba)
            json = iotrace.parse_json(histogram)
            TestRun.LOGGER.info(str(json[0]['histogram'][0]))
            TestRun.LOGGER.info(str(json[0]['histogram'][0]['total']['range'][4]))
            for bucket_index in range(number_buckets):
                read = json[0]['histogram'][0]['read']['range'][bucket_index]
                write = json[0]['histogram'][0]['write']['range'][bucket_index]
                discard = json[0]['histogram'][0]['discard']['range'][bucket_index]
                total = json[0]['histogram'][0]['total']['range'][bucket_index]
                if int(read['begin']) != start_lba + bucket_index * bucket_size:
                    raise Exception(f"Invalid read begin range: {read['begin']} for index {bucket_index}")
                if int(read['end']) != start_lba + (bucket_index + 1) * bucket_size - 1:
                    raise Exception(f"Invalid read end range: {read['end']} for index {bucket_index}")
                if int(read['count']) != bucket_index + 1:
                    raise Exception(f"Invalid read count: {read['count']} for index {bucket_index}")

                if int(write['begin']) != start_lba + bucket_index * bucket_size:
                    raise Exception(f"Invalid write begin range: {write['begin']} for index {bucket_index}")
                if int(write['end']) != start_lba + (bucket_index + 1) * bucket_size - 1:
                    raise Exception(f"Invalid write end range: {write['end']} for index {bucket_index}")
                if int(write['count']) != bucket_index + 1:
                    raise Exception(f"Invalid write count: {write['count']} for index {bucket_index}")

                if int(discard['begin']) != start_lba + bucket_index * bucket_size:
                    raise Exception(f"Invalid discard begin range: {discard['begin']} for index {bucket_index}")
                if int(discard['end']) != start_lba + (bucket_index + 1) * bucket_size - 1:
                    raise Exception(f"Invalid discard end range: {discard['end']} for index {bucket_index}")
                if int(discard['count']) != bucket_index + 1:
                    raise Exception(f"Invalid discard count: {discard['count']} for index {bucket_index}")

                if int(total['begin']) != start_lba + bucket_index * bucket_size:
                    raise Exception(f"Invalid total begin range: {total['begin']} for index {bucket_index}")
                if int(total['end']) != start_lba + (bucket_index + 1) * bucket_size - 1:
                    raise Exception(f"Invalid total end range: {total['end']} for index {bucket_index}")
                if int(total['count']) != 3 * (bucket_index + 1):
                    raise Exception(f"Invalid total count: {total['count']} for index {bucket_index}")

#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from core.test_run import TestRun
from test_tools.dd import Dd
from test_tools.fio.fio import Fio
from test_tools.fio.fio_param import IoEngine, ReadWrite
from test_utils.size import Unit, Size
import time
import datetime
from random import randrange
from math import floor

from utils.iotrace import IotracePlugin

# iotrace uses 512B sector size, even if underlying disk has larger sectors
iotrace_lba_len = 512


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
            write_offset = 2 * write_length.get_value()
            dd = (Dd().input("/dev/urandom").output(disk.system_path).count(1).
                  block_size(write_length).oflag('direct,sync').
                  seek(int(write_offset / write_length.get_value())))
            dd.run()
        with TestRun.step("Send read command"):
            read_length = Size(19, disk.block_size)
            read_offset = 2 * read_length.get_value()
            dd = (Dd().input(disk.system_path).output("/dev/null").count(1).
                  block_size(read_length).iflag('direct,sync').
                  skip(int(read_offset / read_length.get_value())))
            dd.run()
        with TestRun.step("Send discard command"):
            discard_length = Size(21, disk.block_size).get_value()
            discard_offset = int(2 * discard_length)
            TestRun.executor.run_expect_success(f"blkdiscard -o {discard_offset}"
                                                f" -l {int(discard_length)} {disk.system_path}")
        with TestRun.step("Stop tracing"):
            iotrace.stop_tracing()
        with TestRun.step("Verify trace correctness"):
            trace_path = IotracePlugin.get_latest_trace_path()
            events_parsed = IotracePlugin.get_trace_events(trace_path)
            result = any(
                'io' in event
                and 'operation' in event['io']
                and event['io']['operation'] == 'Write'
                # LBA 0 events don't have a lba field, so skip them
                and 'lba' in event['io']
                and int(event['io']['lba']) == int(write_offset / iotrace_lba_len)
                and int(event['io']['len']) == int(write_length.get_value() / iotrace_lba_len)
                and f"/dev/{event['device']['name']}" == disk.system_path
                for event in events_parsed)
            if not result:
                TestRun.fail("Could not find write event")
            result = any(
                'io' in event
                and 'operation' in event['io']
                and event['io']['operation'] == 'Read'
                # LBA 0 events don't have a lba field, so skip them
                and 'lba' in event['io']
                and int(event['io']['lba']) == int(read_offset / iotrace_lba_len)
                and int(event['io']['len']) == int(read_length.get_value() / iotrace_lba_len)
                and f"/dev/{event['device']['name']}" == disk.system_path
                for event in events_parsed)
            if not result:
                TestRun.fail("Could not find read event")
            result = any(
                'io' in event
                and 'operation' in event['io']
                and event['io']['operation'] == 'Discard'
                # LBA 0 events don't have a lba field, so skip them
                and 'lba' in event['io']
                and int(event['io']['lba']) == int(discard_offset / iotrace_lba_len)
                and int(event['io']['len']) == int(discard_length / iotrace_lba_len)
                and f"/dev/{event['device']['name']}" == disk.system_path
                for event in events_parsed)
            if not result:
                TestRun.fail("Could not find discard event")


def test_lba_histogram():
    TestRun.LOGGER.info("Testing lba histogram")
    iotrace = TestRun.plugins['iotrace']
    number_buckets = 32
    bucket_size = Size(256)
    start_lba = 10240
    end_lba = start_lba + number_buckets * bucket_size.value
    for disk in TestRun.dut.disks:
        io_len = Size(1, disk.block_size)
        with TestRun.step("Start tracing"):
            iotrace.start_tracing([disk.system_path])
            time.sleep(5)
        with TestRun.step("Send write commands"):
            for bucket_index in range(number_buckets):
                for i in range(bucket_index + 1):
                    # Generate IO for a given bucket. The LBA may need to be
                    # translated from 512B (which iotrace always uses) to 4KiB
                    # (if that's what the underlying disk surfaces)
                    seek = ((start_lba + bucket_index * bucket_size.value
                             + randrange(bucket_size.value))
                            * iotrace_lba_len / disk.block_size.get_value())
                    dd = (Dd().input("/dev/urandom")
                          .output(disk.system_path)
                          .count(1)
                          .block_size(io_len)
                          .oflag('direct,sync')
                          .seek(int(seek)))
                    dd.run()
        with TestRun.step("Send read commands"):
            for bucket_index in range(number_buckets):
                for i in range(bucket_index + 1):
                    seek = ((start_lba + bucket_index * bucket_size.value
                             + randrange(bucket_size.value))
                            * iotrace_lba_len / disk.block_size.get_value())
                    dd = (Dd().input(disk.system_path)
                          .output("/dev/null")
                          .count(1)
                          .block_size(io_len)
                          .iflag('direct,sync')
                          .skip(int(seek)))
                    dd.run()
        with TestRun.step("Send discard commands"):
            for bucket_index in range(number_buckets):
                for i in range(bucket_index + 1):
                    seek = (start_lba + bucket_index * bucket_size.value
                            + randrange(bucket_size.value))
                    seek = round_down(seek * iotrace_lba_len, disk.block_size.get_value())
                    TestRun.executor.run_expect_success(
                        f"blkdiscard -o {int(seek)} "
                        f"-l {disk.block_size.get_value()} {disk.system_path}")
        with TestRun.step("Stop tracing"):
            iotrace.stop_tracing()
        with TestRun.step("Verify histogram correctness"):
            trace_path = IotracePlugin.get_latest_trace_path()
            json = IotracePlugin.get_lba_histogram(trace_path, bucket_size, start_lba, end_lba)
            TestRun.LOGGER.info(str(json[0]['histogram'][0]))
            TestRun.LOGGER.info(str(json[0]['histogram'][0]['total']['range'][4]))
            for bucket_index in range(number_buckets):
                read = json[0]['histogram'][0]['read']['range'][bucket_index]
                write = json[0]['histogram'][0]['write']['range'][bucket_index]
                discard = json[0]['histogram'][0]['discard']['range'][bucket_index]
                total = json[0]['histogram'][0]['total']['range'][bucket_index]
                if int(read['begin']) != start_lba + bucket_index * bucket_size.value:
                    TestRun.fail(
                        f"Invalid read begin range: {read['begin']} for index {bucket_index}")
                if int(read['end']) != start_lba + (bucket_index + 1) * bucket_size.value - 1:
                    TestRun.fail(
                        f"Invalid read end range: {read['end']} for index {bucket_index}")
                if int(read['count']) != bucket_index + 1:
                    TestRun.fail(
                        f"Invalid read count: {read['count']} for index {bucket_index}")
                if int(write['begin']) != start_lba + bucket_index * bucket_size.value:
                    TestRun.fail(
                        f"Invalid write begin range: {write['begin']} for index {bucket_index}")
                if int(write['end']) != start_lba + (bucket_index + 1) * bucket_size.value - 1:
                    TestRun.fail(
                        f"Invalid write end range: {write['end']} for index {bucket_index}")
                if int(write['count']) != bucket_index + 1:
                    TestRun.fail(
                        f"Invalid write count: {write['count']} for index {bucket_index}")
                if int(discard['begin']) != start_lba + bucket_index * bucket_size.value:
                    TestRun.fail(
                        f"Invalid discard begin range: {discard['begin']} "
                        f"for index {bucket_index}")
                if int(discard['end']) != start_lba + (bucket_index + 1) * bucket_size.value - 1:
                    TestRun.fail(
                        f"Invalid discard end range: {discard['end']} for index {bucket_index}")
                if int(discard['count']) != bucket_index + 1:
                    TestRun.fail(
                        f"Invalid discard count: {discard['count']} for index {bucket_index}")
                if int(total['begin']) != start_lba + bucket_index * bucket_size.value:
                    TestRun.fail(
                        f"Invalid total begin range: {total['begin']} for index {bucket_index}")
                if int(total['end']) != start_lba + (bucket_index + 1) * bucket_size.value - 1:
                    TestRun.fail(
                        f"Invalid total end range: {total['end']} for index {bucket_index}")
                if int(total['count']) != 3 * (bucket_index + 1):
                    TestRun.fail(
                        f"Invalid total count: {total['count']} for index {bucket_index}")


def test_verify_flushes():
    TestRun.LOGGER.info("Testing io events during tracing")
    iotrace = TestRun.plugins['iotrace']
    for disk in TestRun.dut.disks:
        with TestRun.step("Start tracing"):
            iotrace.start_tracing([disk.system_path])
            time.sleep(5)
        with TestRun.step("Run test workload with sync"):
            fio = (
                Fio().create_command().io_engine(IoEngine.sync).
                    block_size(Size(4, Unit.KibiByte)).time_based().
                    read_write(ReadWrite.randwrite).
                    target(disk.system_path).direct(value=False).
                    run_time(datetime.timedelta(seconds=10)).fsync(value=1))
            fio.run()
        with TestRun.step("Stop tracing"):
            iotrace.stop_tracing()
        with TestRun.step("Verify trace correctness"):
            trace_path = IotracePlugin.get_latest_trace_path()
            events_parsed = IotracePlugin.get_trace_events(trace_path)
            result = any('io' in event and 'fua' in event['io']
                         and bool(event['io']['fua'] is True)
                         for event in events_parsed)
            if not result:
                TestRun.fail("Could not find event with fua")
            result = any('io' in event and 'flush' in event['io']
                         and bool(event['io']['flush'] is True)
                         for event in events_parsed)
            if not result:
                TestRun.fail("Could not find event with flush")
            result = all('flush' in event['io'] and
                         bool(event['io']['flush']) is True
                         for event in
                         filter(lambda event:
                                'io' in event and 'lba' in event['io'] and
                                int(event['io']['lba']) == 0 and
                                int(event['io']['len']) == 0, events_parsed))
            if not result:
                TestRun.fail("All events with lba 0 and len 0 should have flush")

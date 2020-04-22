#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import time
import pytest

from api.iotrace_lat_hist_parser import LatencyHistograms
from utils.iotrace import IotracePlugin
from core.test_run import TestRun
from test_tools.fio.fio import Fio
from test_tools.fio.fio_param import ReadWrite, IoEngine, FioOutput
from test_utils.size import Size, Unit
from test_utils.os_utils import Udev


@pytest.mark.parametrize("io_dir", [ReadWrite.write, ReadWrite.read])
def test_latency_histogram_basic(io_dir):
    """
        title: Test for basic latency histogram properties
        description: |
            Test if samples count reported by fio equals count from iotracer (taking
            into consideration dropped ones).
        pass_criteria:
            - Fio's samples number equals number of iotracer samples + number of
              dropped ones
    """
    read = ReadWrite.read is io_dir
    iotrace = TestRun.plugins['iotrace']

    for disk in TestRun.dut.disks:
        with TestRun.step(f"Start tracing on {disk.system_path}"):
            tracer = iotrace.start_tracing([disk.system_path])
            time.sleep(3)

        with TestRun.step(f"Run {io_dir} IO"):
            fio = (Fio().create_command(output_type=FioOutput.jsonplus)
                   .io_engine(IoEngine.libaio)
                   .size(Size(300, Unit.MebiByte))
                   .block_size(Size(1, Unit.Blocks4096))
                   .read_write(io_dir)
                   .target(disk.system_path)
                   .direct())
            fio_out = fio.run()

        with TestRun.step("Stop tracing"):
            iotrace.stop_tracing()

        with TestRun.step("Get latency histogram from iotracer"):
            trace_path = iotrace.get_latest_trace_path()
            iot_histograms = iotrace.get_latency_histograms(trace_path)
            iot_histo = iot_histograms.read if read else iot_histograms.write

        with TestRun.step("Build histogram from fio bins"):
            fio_histograms = LatencyHistograms.build_histo_from_fio_job(
                iot_histograms, fio_out[0].job, io_dir)
            fio_histo = fio_histograms.read if read else fio_histograms.write

        with TestRun.step("Check if count of samples reported by iotracer "
                          "(including dropped ones) equals count of fio samples "):
            summary = iotrace.get_trace_summary(trace_path)
            dropped_events = int(summary['droppedEvents'])

            iot_samples_count = iot_histo.samples_count() + dropped_events
            fio_samples_count = fio_histo.samples_count()

            if iot_samples_count != fio_samples_count:
                TestRun.fail(f"Wrong samples count reported by iotracer "
                             f"compared to fio's ({fio_samples_count} samples). "
                             f"Iotracer samples count: {iot_histo.samples_count()} "
                             f"Iotracer dropped samples count: {dropped_events}")


@pytest.mark.parametrize("io_dir", [ReadWrite.write, ReadWrite.read])
def test_latency_histogram_correlation(io_dir):
    """
        title: Test for comparing fio and iotracer latency histogram
        description: |
            Compare fio latency histogram with iotracer one by calculating
            correlation.
        pass_criteria:
            - Correlation factor should not be less than 90%
    """
    read = ReadWrite.read is io_dir
    iotrace = TestRun.plugins['iotrace']

    for disk in TestRun.dut.disks:
        with TestRun.step(f"Start tracing on {disk.system_path}"):
            tracer = iotrace.start_tracing([disk.system_path])
            time.sleep(3)

        with TestRun.step(f"Run {io_dir} IO"):
            fio = (Fio().create_command(output_type=FioOutput.jsonplus)
                   .io_engine(IoEngine.libaio)
                   .size(Size(1, Unit.GibiByte))
                   .block_size(Size(1, Unit.Blocks4096))
                   .read_write(io_dir)
                   .target(disk.system_path)
                   .direct())
            fio_out = fio.run()

        with TestRun.step("Stop tracing"):
            iotrace.stop_tracing()

        with TestRun.step("Get latency histogram from iotracer"):
            trace_path = iotrace.get_latest_trace_path()

            iot_histograms = iotrace.get_latency_histograms(trace_path)
            iot_histo = iot_histograms.read if read else iot_histograms.write

        with TestRun.step("Get average latency reported by iotracer"):
            trace_stats = iotrace.get_trace_statistics(trace_path)
            iot_avg_lat = (int(trace_stats[0]['read']['latency']['average']) if
                           read else
                           int(trace_stats[0]['write']['latency']['average']))

        with TestRun.step("Compare average latency reported by fio adn iotracer"):
            fio_avg_lat = (int(fio_out[0].job.read.clat_ns.__dict__['mean']) if
                           read else
                           int(fio_out[0].job.write.clat_ns.__dict__['mean']))

            if float(fio_avg_lat) < float(iot_avg_lat):
                TestRun.fail("Average fio latency smaller than average latency "
                             "reported by iotracer!")

        with TestRun.step("Build normalized histogram from fio bins"):
            avg_diff = fio_avg_lat - iot_avg_lat

            fio_histograms = LatencyHistograms.build_histo_from_fio_job(
                iot_histograms, fio_out[0].job, io_dir, avg_diff)
            fio_histo = fio_histograms.read if read else fio_histograms.write

        with TestRun.step("Compare histograms"):
            corr = LatencyHistograms.calc_histo_correlation(fio_histo,
                                                           iot_histo)
            #TODO choose best expected correlation value
            if corr < 0.90:
                #TODO dump all available data (fio bins, traces) here as well
                TestRun.fail(f"Histograms correlations ({corr}) less than expected (90%)!")




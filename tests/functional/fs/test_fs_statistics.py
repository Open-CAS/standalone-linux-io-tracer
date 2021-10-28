#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

import time
import pytest

from core.test_run import TestRun
from test_tools.disk_utils import Filesystem
from test_tools.fs_utils import create_directory
from test_tools.fio.fio import Fio
from test_tools.fio.fio_param import ReadWrite, IoEngine
from test_utils.size import Size, Unit
from api.iotrace_stats_parser import (
    parse_fs_stats,
    DirectoryTraceStatistics,
    FileTraceStatistics,
    ExtensionTraceStatistics,
)
from utils.iotrace import IotracePlugin


@pytest.fixture(params=Filesystem)
def fs(request):
    file_system = request.param
    mountpoint = "/mnt"
    disk = TestRun.dut.disks[0]

    with TestRun.step(f"Create file system {file_system}"):
        disk.create_filesystem(file_system)

    with TestRun.step("Mount device"):
        disk.mount(mountpoint)

    yield disk, mountpoint

    with TestRun.step("Unmount device"):
        disk.unmount()


def test_fs_statistics(fs):
    """
        title: Test if FS statistics are properly calculated by iotrace
        description: |
          Create files on filesystem (relative to fs root):
            * test_dir/A.x
            * test_dir/B.y
            * A.y
            * B.x
          and execute known workload on them. Each file has different size and is written with
          different block size, also varying number of times. This way number of loops on given job
          is equal to WiF, and size of given file is equal to workset. Only workset and WiF stats
          are verified.
        pass_criteria:
          - Statistics for file prefixes A and B match with expected values
          - Statistics for file extensions x and y match with expected values
          - Statistics for root fs directory and test_dir directory match with expected values
    """

    iotrace = TestRun.plugins["iotrace"]

    (disk, filesystem) = fs

    with TestRun.step("Prepare fio configuration"):
        test_dir = f"{filesystem}/test_dir"

        fio_cfg = (
            Fio()
            .create_command()
            .io_engine(IoEngine.libaio)
            .sync(True)
            .read_write(ReadWrite.write)
        )

        dir_Ax_size = Size(3, Unit.MebiByte)
        dir_Ax_WiF = 2
        dir_Ax_written = dir_Ax_size * dir_Ax_WiF

        (
            fio_cfg.add_job("test_dir/A.x")
            .target(f"{test_dir}/A.x")
            .file_size(dir_Ax_size)
            .block_size(Size(4, Unit.KibiByte))
            .loops(dir_Ax_WiF)
        )

        dir_By_size = Size(16, Unit.KibiByte)
        dir_By_WiF = 2
        dir_By_written = dir_By_size * dir_By_WiF

        (
            fio_cfg.add_job("test_dir/B.y")
            .target(f"{test_dir}/B.y")
            .file_size(dir_By_size)
            .block_size(Size(16, Unit.KibiByte))
            .loops(dir_By_WiF)
        )

        Ay_size = Size(2, Unit.MebiByte)
        Ay_WiF = 5
        Ay_written = Ay_size * Ay_WiF

        (
            fio_cfg.add_job("A.y")
            .target(f"{filesystem}/A.y")
            .file_size(Ay_size)
            .block_size(Size(64, Unit.KibiByte))
            .loops(Ay_WiF)
        )

        Bx_size = Size(5, Unit.MebiByte)
        Bx_WiF = 1
        Bx_written = Bx_size * Bx_WiF

        (
            fio_cfg.add_job("B.x")
            .target(f"{filesystem}/B.x")
            .file_size(Bx_size)
            .block_size(Size(128, Unit.KibiByte))
        )

    with TestRun.step("Prepare directory and files for test"):
        create_directory(test_dir)

        # In this run FIO will only create all the files needed by jobs and quit.
        # If we didn't do it WiF would be +1 for each of the files (one file write on creation).
        # For simplicity of calculations we create files first and after that start tracing.
        fio_cfg.edit_global().create_only(True)
        fio_cfg.run()

    with TestRun.step("Start tracing"):
        iotrace.start_tracing([disk.system_path], Size(1, Unit.GibiByte))
        time.sleep(3)

    with TestRun.step("Run workload"):
        fio_cfg.edit_global().create_only(False)
        fio_cfg.run()

    with TestRun.step("Stop tracing"):
        iotrace.stop_tracing()

    with TestRun.step("Verify trace correctness"):
        A_prefix_workset = dir_Ax_size + Ay_size
        B_prefix_workset = dir_By_size + Bx_size

        A_prefix_WiF = (dir_Ax_written + Ay_written) / A_prefix_workset
        B_prefix_WiF = (dir_By_written + Bx_written) / B_prefix_workset

        x_extension_workset = dir_Ax_size + Bx_size
        y_extension_workset = dir_By_size + Ay_size

        x_extension_WiF = (dir_Ax_written + Bx_written) / x_extension_workset
        y_extension_WiF = (Ay_written + dir_By_written) / y_extension_workset

        test_dir_workset = dir_Ax_size + dir_By_size
        root_dir_workset = Ay_size + Bx_size

        test_dir_WiF = (dir_Ax_written + dir_By_written) / test_dir_workset
        root_dir_WiF = (Ay_written + Bx_written) / root_dir_workset

        trace_path = IotracePlugin.get_latest_trace_path()
        stats = parse_fs_stats(IotracePlugin.get_fs_statistics(trace_path)[0]['entries'])

        prefix_stats = {
            stat.file_name_prefix: stat for stat in stats if type(stat) == FileTraceStatistics
        }
        extension_stats = {
            stat.extension: stat for stat in stats if type(stat) == ExtensionTraceStatistics
        }
        dir_stats = {
            stat.directory: stat for stat in stats if type(stat) == DirectoryTraceStatistics
        }

        for (desc, (expect_workset, expect_WiF), got) in [
            ("A file prefix", (A_prefix_workset, A_prefix_WiF), prefix_stats["A"]),
            ("B file prefix", (B_prefix_workset, B_prefix_WiF), prefix_stats["B"]),
            ("x file extension", (x_extension_workset, x_extension_WiF), extension_stats["x"]),
            ("y file extension", (y_extension_workset, y_extension_WiF), extension_stats["y"]),
            ("test_dir directory", (test_dir_workset, test_dir_WiF), dir_stats["/test_dir"]),
            ("root directory", (root_dir_workset, root_dir_WiF), dir_stats["/"]),
        ]:
            expect_equal(f"{desc} workset", expect_workset, got.statistics.total.metrics.workset)
            expect_equal(
                f"{desc} write invalidation factor", expect_WiF, got.statistics.write.metrics.wif
            )


def expect_equal(what: str, expected: int, got: int):
    if expected != got:
        TestRun.LOGGER.error(f"Invalid {what} (expected {expected}, got {got})")

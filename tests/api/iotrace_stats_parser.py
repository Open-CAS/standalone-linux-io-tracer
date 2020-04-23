#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from typing import List

from test_utils.size import Size, Unit, UnitPerSecond, parse_unit
from test_utils.time import Time
from utils.iotrace import IotracePlugin

class BaseTraceStatistics:
    def __init__(self, trace):
        self.duration = int(trace["duration"])
        self.read = IOStatistics(trace["read"])
        self.write = IOStatistics(trace["write"])
        self.total = IOStatistics(trace["total"])

    def __str__(self):
        ret = f"duration: {self.duration}\n"
        ret += f"READ: {self.read}"
        ret += f"WRITE: {self.write}"
        ret += f"TOTAL: {self.total}"

        return ret


class DeviceTraceStatistics(BaseTraceStatistics):
    def __init__(self, trace):
        super().__init__(trace)
        self.device = DeviceStats(trace["desc"]["device"])
        self.discard = ServiceStats(trace["discard"])
        self.flush = ServiceStats(trace["flush"])

    def __str__(self):
        ret = f"{self.device}"
        ret += super().__str__()
        ret += f"DISCARD: {self.discard}"
        ret += f"FLUSH: {self.flush}"

        return ret


class FsTraceStatistics:
    def __init__(self, trace):
        self.device_id = trace["deviceId"]
        self.partition_id = trace["partitionId"]
        self.statistics = BaseTraceStatistics(trace["statistics"])

    def __str__(self):
        ret = f"Device ID: {self.device_id}"
        ret += f"Partition ID: {self.partition_id}"
        ret += str(self.statistics)

        return ret


class DirectoryTraceStatistics(FsTraceStatistics):
    def __init__(self, trace):
        super().__init__(trace)
        self.directory = trace["directory"]

    def __str__(self):
        ret = f"Directory: {self.directory}"
        ret += super().__str__()

        return ret


class FileTraceStatistics(FsTraceStatistics):
    def __init__(self, trace):
        super().__init__(trace)
        self.file_name_prefix = trace["fileNamePrefix"]

    def __str__(self):
        ret = f"File name prefix: {self.file_name_prefix}"
        ret += super().__str__()

        return ret


class ExtensionTraceStatistics(FsTraceStatistics):
    def __init__(self, trace):
        super().__init__(trace)
        self.extension = trace["fileExtension"]

    def __str__(self):
        ret = f"Extension: {self.extension}"
        ret += super().__str__()

        return ret


def parse_fs_stats(stats: list) -> List[FsTraceStatistics]:
    parsed = []

    for entry in stats:
        if "directory" in entry:
            parsed.append(DirectoryTraceStatistics(entry))
        elif "fileExtension" in entry:
            parsed.append(ExtensionTraceStatistics(entry))
        elif "fileNamePrefix" in entry:
            parsed.append(FileTraceStatistics(entry))
        else:
            raise Exception("Unrecognized FS statistics")

    return parsed


class DeviceStats:
    def __init__(self, trace):
        self.id = trace["id"]
        self.name = trace["name"]
        self.size = Size(float(trace["size"]), Unit.Blocks512)

    def __str__(self):
        return f"device: id: {self.id}, name: {self.name}, size {self.size}\n"


class IOStatistics:
    def __init__(self, trace):
        self.size = SectorStat(trace["size"])
        self.latency = TimeStat(trace["latency"]) if "latency" in trace else None
        self.metrics = Metrics(trace["metrics"])
        self.errors = int(trace["errors"]) if "errors" in trace else None
        self.count = int(trace["count"])

    def __str__(self):
        ret = f"{self.size}"
        ret += f"{self.latency}" if self.latency else ""
        ret += f"{self.metrics}"
        ret += f"{self.errors}" if self.errors else ""
        ret += f"count: {self.count}\n"

        return ret


class TimeStat:
    def __init__(self, trace):
        self.average = Time(nanoseconds=int(trace["average"]))
        self.min = Time(nanoseconds=int(trace["min"]))
        self.max = Time(nanoseconds=int(trace["max"]))
        self.total = Time(nanoseconds=int(trace["total"]))
        self.percentiles = Percentiles(
            trace["percentiles"], "of reqs have latency lower than"
        )

    def __str__(self):
        ret = f"average latency: {self.average.total_nanoseconds()} ns\n"
        ret += f"min latency: {self.min.total_nanoseconds()} ns\n"
        ret += f"max latency: {self.max.total_nanoseconds()} ns\n"
        ret += f"total: {self.total.total_nanoseconds()} ns\n"
        ret += f"{self.percentiles}"

        return ret


class SectorStat:
    def __init__(self, trace):
        self.average = Size(float(trace["average"]), Unit.Blocks512)
        self.min = Size(float(trace["min"]), Unit.Blocks512)
        self.max = Size(float(trace["max"]), Unit.Blocks512)
        self.total = Size(float(trace["total"]), Unit.Blocks512)
        self.percentiles = Percentiles(
            trace["percentiles"], "of reqs touches no more sectors than"
        )

    def __str__(self):
        ret = f"average size: {self.average}\n"
        ret += f"min size: {self.min}\n"
        ret += f"max size: {self.max}\n"
        ret += f"total: {self.total}\n"
        ret += f"{self.percentiles}"

        return ret


class ServiceStats:
    def __init__(self, trace):
        self.metrics = Metrics(trace["metrics"])
        self.latency = TimeStat(trace["latency"])
        self.errors = int(trace["errors"])

    def __str__(self):
        ret = f"metrics {self.metrics}"
        ret += f"errors {self.errors}\n"

        return ret


class Percentiles:
    def __init__(self, trace, message="of population have value lower than"):
        self.message = message

        self.le90 = trace.get("90.000000th", 0)
        self.le99 = trace.get("99.000000th", 0)
        self.le99_90 = trace.get("99.900000th", 0)
        self.le99_99 = trace.get("99.990000th", 0)

    def __str__(self):
        ret = f"90% {self.message} {self.le90}\n"
        ret += f"99% {self.message} {self.le99}\n"
        ret += f"99.9% {self.message} {self.le99_90}\n"
        ret += f"99.99% {self.message} {self.le99_99}\n"

        return ret


class Metrics:
    def __init__(self, trace):
        try:
            self.iops = float(trace["throughput"]["value"])
            self.workset = Size(
                float(trace["workset"]["value"]), Unit.Blocks512
            )
            bandwidth_unit = parse_unit(
                trace["bandwidth"]["unit"].split("/")[0]
            )
            bandwidth_value = float(trace["bandwidth"]["value"])
            self.bandwidth = Size(bandwidth_value, UnitPerSecond(bandwidth_unit))
        except KeyError:
            self.iops = 0
            self.workset = Size(0)
            self.bandwidth = Size(0)

        if "write invalidation factor" in trace:
            self.wif = float(trace["write invalidation factor"]["value"])
        else:
            self.wif = None

    def __str__(self):
        ret = f"metrics: IOPS: {self.iops}\n"
        ret += f"workset: {self.workset}\n"
        if self.wif:
            ret += f"write invalidation factor: {self.wif}\n"
        ret += f"bandwidth: {self.bandwidth}\n"

        return ret

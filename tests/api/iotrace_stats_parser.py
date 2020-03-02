#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest

from test_utils.size import Size, Unit, parse_unit
from core.test_run import TestRun

from iotrace import IotracePlugin


class TraceStatistics:
    def __init__(self, trace):
        self.device = DeviceStats(trace["desc"]["device"])
        self.duration = int(trace["duration"])
        self.read = IOStatistics(trace["read"])
        self.write = IOStatistics(trace["write"])
        self.discard = ServiceStats(trace["discard"])
        self.flush = ServiceStats(trace["flush"])
        self.total = IOStatistics(trace["total"])

    def __str__(self):
        ret = f"{self.device}"
        ret += f"duration: {self.duration}\n"
        ret += f"READ: {self.read}"
        ret += f"WRITE: {self.write}"
        ret += f"DISCARD: {self.discard}"
        ret += f"FLUSH: {self.flush}"
        ret += f"TOTAL: {self.total}"

        return ret


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
        self.latency = TimeStat(trace["latency"])
        self.metrics = Metrics(trace["metrics"])
        self.errors = int(trace["errors"])

    def __str__(self):
        ret = f"{self.size}"
        ret += f"{self.latency}"
        ret += f"{self.metrics}"
        ret += f"errors {self.errors}"

        return ret


class TimeStat:
    def __init__(self, trace):
        # TODO class with nanoseconds needed
        self.average = int(trace["average"])
        self.min = int(trace["min"])
        self.max = int(trace["max"])
        self.count = int(trace["count"])
        self.total = int(trace["total"])
        self.percentiles = Percentiles(
            trace["percentiles"], "of reqs have latency lower than"
        )

    def __str__(self):
        ret = f"average latency: {self.average} ns\n"
        ret += f"min latency: {self.min} ns\n"
        ret += f"max latency: {self.max} ns\n"
        ret += f"count: {self.count}\n"
        ret += f"total: {self.total} ns\n"
        ret += f"{self.percentiles}"

        return ret


class SectorStat:
    def __init__(self, trace):
        self.average = Size(float(trace["average"]), Unit.Blocks512)
        self.min = Size(float(trace["min"]), Unit.Blocks512)
        self.max = Size(float(trace["max"]), Unit.Blocks512)
        self.count = Size(float(trace["count"]), Unit.Blocks512)
        self.total = Size(float(trace["total"]), Unit.Blocks512)
        self.percentiles = Percentiles(
            trace["percentiles"], "of reqs touches no more sectors than"
        )

    def __str__(self):
        ret = f"average size: {self.average}\n"
        ret += f"min size: {self.min}\n"
        ret += f"max size: {self.max}\n"
        ret += f"count: {self.count}\n"
        ret += f"total: {self.total}\n"
        ret += f"{self.percentiles}"

        return ret


class ServiceStats:
    def __init__(self, trace):
        self.metrics = Metrics(trace["metrics"])
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
            self.bandwidth_per_sec = Size(bandwidth_value, bandwidth_unit)
        except KeyError:
            self.iops = 0
            self.workset = Size(0)
            self.bandwidth_per_sec = Size(0)

    def __str__(self):
        ret = f"metrics: IOPS: {self.iops}\n"
        ret += f"workset: {self.workset}\n"
        ret += f"bandwidth: {self.bandwidth_per_sec}/s\n"

        return ret

#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from aenum import Enum, auto


class HistoTypes(Enum):
    read = auto()
    write = auto()
    discard = auto()
    flush = auto()
    total = auto()


class LatencyHistogram:
    def __init__(self, hist):
        self.duration = hist['duration']
        self.dev_id = hist['desc']['device']['id']
        self.dev_name = hist['desc']['device']['name']
        self.dev_size = hist['desc']['device']['size']

        self.write = Histogram(HistoTypes.write,
                               hist['write']['range'],
                               hist['write']['unit'])
        self.read = Histogram(HistoTypes.read,
                              hist['read']['range'],
                              hist['write']['unit'])
        self.discard = Histogram(HistoTypes.discard,
                                 hist['discard']['range'],
                                 hist['discard']['unit'])
        self.flush = Histogram(HistoTypes.flush,
                               hist['flush']['range'],
                               hist['flush']['unit'])
        self.total = Histogram(HistoTypes.total,
                               hist['total']['range'],
                               hist['total']['unit'])

    def __str__(self):
        ret = f"Duration: {self.duration}\n"
        ret += f"Device id: {self.dev_id}\n"
        ret += f"Device name: {self.dev_name}\n"
        ret += f"Device size: {self.dev_size}\n"
        ret += f"{self.write}\n"
        ret += f"{self.read}\n"
        ret += f"{self.discard}\n"
        ret += f"{self.flush}\n"
        ret += f"{self.total}\n"

        return ret


class Histogram:
    def __init__(self, type: HistoTypes, range: dict(), unit):
        self.type = type
        self.unit = unit
        self.items = []
        for i in range:
            self.items.append(HistogramEntry(i))

    def __str__(self):
        ret = f"Histogram type: {self.type.name}\n"
        ret += f"Unit: {self.unit}\n"
        ret += f"Histogram items:\n"
        for i in self.items:
            ret += f"\t{i}\n"

        return ret


class HistogramEntry:
    def __init__(self, entry: dict):
        self.begin = entry['begin']
        self.end = entry['end']
        self.count = entry['count']

    def __str__(self):
        return f"begin: {self.begin}\tend: {self.end}\tcount: {self.count}"

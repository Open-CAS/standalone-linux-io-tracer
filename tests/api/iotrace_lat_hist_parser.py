#
# Copyright(c) 2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from aenum import Enum, auto
from math import sqrt
import copy

from test_tools.fio.fio_param import ReadWrite


class HistoTypes(Enum):
    read = auto()
    write = auto()
    discard = auto()
    flush = auto()
    total = auto()


class LatencyHistograms:
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

    @staticmethod
    def build_histo_from_fio_job(reference_hist, fio_job, io_dir, avg_lat_diff=None):
        hist_copy = copy.deepcopy(reference_hist)
        fio_hist = (hist_copy.write if
                    io_dir is ReadWrite.write else
                    hist_copy.read)

        for i in fio_hist.items:
            i.count = 0

        fio_job_bins = (fio_job.write.clat_ns.__dict__['bins'].__dict__ if
                        io_dir is ReadWrite.write else
                        fio_job.read.clat_ns.__dict__['bins'].__dict__)

        if avg_lat_diff is not None:
            fio_normalized_bins = dict()
            # offset fio samples by 'average' latency
            for key in fio_job_bins:
                fio_lat = int(key)
                fio_lat_offset = fio_lat - avg_lat_diff
                fio_normalized_bins[fio_lat_offset] = fio_job_bins[key]
                fio_bins = fio_normalized_bins
        else:
            fio_bins = fio_job_bins

        for bin_key in fio_bins:
            for i, item in enumerate(fio_hist.items):
                if int(bin_key) >= item.begin and int(bin_key) <= item.end:
                    fio_hist.items[i].count += fio_bins[bin_key]
                    break

        return hist_copy

    @staticmethod
    def calc_histo_correlation(fio_hist, iotracer_hist):
        fio_ranges_no = len(fio_hist.items)
        iot_ranges_no = len(iotracer_hist.items)

        if (not fio_ranges_no or not iot_ranges_no or
            fio_ranges_no != iot_ranges_no):
            raise Exception(f"Wrong histogram ranges number for fio "
                            f"({fio_ranges_no}) or iotracer ({iot_ranges_no})")

        fio_bins_count = 0
        for i in fio_hist.items:
            fio_bins_count += i.count

        iot_bins_count = 0
        for i in iotracer_hist.items:
            iot_bins_count += i.count

        fio_avg = fio_bins_count / fio_ranges_no
        iot_avg = iot_bins_count / iot_ranges_no

        numerator = 0
        denominator = 0

        for fio_bin, iot_bin in zip(fio_hist.items, iotracer_hist.items):
            numerator += (fio_bin.count - fio_avg) * (iot_bin.count - iot_avg)

        fio_sum_avg = 0
        iot_sum_avg = 0
        for fio_bin, iot_bin in zip(fio_hist.items, iotracer_hist.items):
            fio_sum_avg += (fio_bin.count - fio_avg)**2
            iot_sum_avg += (iot_bin.count - iot_avg)**2

        denominator = sqrt(fio_sum_avg * iot_sum_avg)

        corr = numerator / denominator

        return corr

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

    def samples_count(self):
        count = 0
        for sample in self.items:
            count += sample.count

        return count

    def __eq__(self, other):
        if (self.type is not other.type or
            self.unit != other.unit or
            len(self.items) != len(other.items)):
            return False

        for item, other_item in zip(self.items, other.items):
            if item != other_item:
                return False

        return True

    def __str__(self):
        ret = f"Histogram type: {self.type.name}\n"
        ret += f"Unit: {self.unit}\n"
        ret += f"Number of samples: {self.samples_count()}\n"
        ret += f"Histogram items:\n"
        for i in self.items:
            ret += f"\t{i}\n"

        return ret


class HistogramEntry:
    def __init__(self, entry: dict):
        self.begin = int(entry['begin'])
        self.end = int(entry['end'])
        self.count = int(entry['count'])

    def __eq__(self, other):
        if (self.begin != other.begin or
            self.end != other.end or
            self.count != other.count):
                return False
        return True

    def __str__(self):
        return "begin: {:<8d} end:{:<8d} count:{:<8d}".format(self.begin,
                                                            self.end,
                                                            self.count)

/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "KernelTraceExecutor.h"

#include <blkid/blkid.h>
#include <bpf/libbpf.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/perf_event.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <third_party/safestringlib.h>
#include <fstream>
#include <thread>
#include <octf/interface/TraceConverter.h>
#include <octf/utils/Exception.h>
#include <octf/utils/FileOperations.h>
#include <octf/utils/Log.h>
#include <octf/utils/SignalHandler.h>
#include "KernelRingTraceProducer.h"
#include "iotrace.bpf.common.h"
#include "iotrace.skel.h"

namespace octf {

static int libbpf_print_fn(enum libbpf_print_level level,
                           const char *format,
                           va_list args) {
    char buffer[1024];

    vsnprintf(buffer, sizeof(buffer), format, args);

    switch (level) {
    case LIBBPF_DEBUG:
        // log::debug << buffer << std::endl;
        break;
    case LIBBPF_INFO:
        // log::verbose << buffer << std::endl;
        break;
    case LIBBPF_WARN:
        std::cout << buffer << std::endl;
        break;
    }

    return 0;
}

KernelTraceExecutor::KernelTraceExecutor(
        const std::vector<std::string> &devices,
        uint32_t ringSizeMiB)
        : m_traceQueueCount(std::thread::hardware_concurrency())
        , m_bpf(nullptr)
        , m_bpfPerf(nullptr)
        , m_bpfThread()
        , m_traceProducerRings(m_traceQueueCount)
        , m_devList(std::make_shared<KernelRingDevList>())
        , m_refSeqId(std::make_shared<KernelRingSeqId>())
        , m_running(true) {
    initDeviceList(devices);

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
    libbpf_set_print(libbpf_print_fn);

    /* Open BPF application */
    m_bpf = iotrace_bpf__open();
    if (!m_bpf) {
        throw Exception("Cannot open BPF iotrace program");
    }
}

KernelTraceExecutor::~KernelTraceExecutor() {
    destroyBpf();
}

bool KernelTraceExecutor::startTrace() {
    /* Load & verify BPF programs */
    int result = iotrace_bpf__load(m_bpf);
    if (result) {
        throw Exception("Cannot load BPF iotrace program");
    }

    /* Parameterize BPF program */
    m_bpf->bss->ref_sid = *m_refSeqId;
    uint64_t i = 0;
    uint64_t count = sizeof(m_bpf->bss->device) / sizeof(m_bpf->bss->device[0]);
    const auto &devList = *m_devList;
    auto iter = devList.begin();

    for (; i < count && iter != devList.end(); i++, iter++) {
        m_bpf->bss->device[i] = iter->id;
    }

    /* Set perf buffer */
    struct perf_buffer_opts pb_opts = {};

    /*
     * Set up ring buffer polling
     * TODO(mbraczak) use the handler of lost trace event count
     */
    pb_opts.sample_cb = perfEventHandler;
    pb_opts.lost_cb = perfEventLost;
    pb_opts.ctx = this;

    m_bpfPerf = perf_buffer__new(bpf_map__fd(m_bpf->maps.events),
                                 256 /* 1MiB per CPU */, &pb_opts);
    if (libbpf_get_error(m_bpfPerf)) {
        m_bpfPerf = nullptr;
        log::cerr << "Cannot setup perf buffer" << std::endl;
        return false;
    }

    /* Attach trace points handlers */
    result = iotrace_bpf__attach(m_bpf);
    if (result) {
        log::cerr << "Cannot load BPF iotrace program" << std::endl;
        return false;
    }

    // Start thread polling on perf event buffer
    m_bpfThread = std::thread([this]() {
        while (m_running) {
            int err = perf_buffer__poll(m_bpfPerf, 100);

            if (err == -EINTR) {
                break;
            }
            if (err < 0) {
                /* TODO(mbarczak) Propagate error and fail trace */
                log::cerr << "Error polling trace event perf buffer";
                break;
            }
        }
    });

    return true;
}

bool KernelTraceExecutor::stopTrace() {
    m_running = false;
    SignalHandler::get().sendSignal(SIGTERM);

    if (m_bpfThread.joinable()) {
        m_bpfThread.join();
    }

    destroyBpf();

    return true;
}

uint32_t KernelTraceExecutor::getTraceQueueCount() {
    return m_traceQueueCount;
}

std::unique_ptr<IRingTraceProducer> KernelTraceExecutor::createProducer(
        uint32_t queue) {
    if (queue >= m_traceProducerRings.size()) {
        throw Exception("Invalid queue id when creating trace producer");
    }

    m_traceProducerRings[queue] = std::make_shared<KernelRingTraceBuffer>();
    m_traceProducerRings[queue]->devs = m_devList;
    m_traceProducerRings[queue]->refSeqId = m_refSeqId;

    auto producer = std::unique_ptr<IRingTraceProducer>(
            new KernelRingTraceProducer(m_traceProducerRings[queue], queue));

    return producer;
}

std::unique_ptr<ITraceConverter> KernelTraceExecutor::createTraceConverter() {
    return std::unique_ptr<TraceConverter>(new TraceConverter());
}

void KernelTraceExecutor::waitUntilStopTrace() {
    // Register signal handler for SIGINT and SIGTERM
    SignalHandler::get().registerSignal(SIGINT);
    SignalHandler::get().registerSignal(SIGTERM);
    SignalHandler::get().wait();
}

void KernelTraceExecutor::perfEventLost(void *ctx,
                                        int cpu,
                                        long long unsigned int lost) {
    auto executor = static_cast<KernelTraceExecutor *>(ctx);

    if (cpu < executor->m_traceQueueCount) {
        executor->m_traceProducerRings[cpu]->lostTrace(lost);
    } else {
        log::cerr << "Invalid CPU number" << std::endl;
    }
}

void KernelTraceExecutor::perfEventHandler(void *ctx,
                                           int cpu,
                                           void *data,
                                           unsigned int data_sz) {
    auto executor = static_cast<KernelTraceExecutor *>(ctx);
    auto hdr = static_cast<iotrace_event_hdr *>(data);

    if (cpu < executor->m_traceQueueCount && sizeof(*hdr) < data_sz &&
        hdr->size < data_sz) {
        executor->m_traceProducerRings[cpu]->pushTrace(data, hdr->size);
    } else {
        log::cerr << "Invalid CPU number" << std::endl;
    }
}

void KernelTraceExecutor::destroyBpf() {
    if (m_bpfPerf) {
        perf_buffer__free(m_bpfPerf);
        m_bpfPerf = nullptr;
    }

    if (m_bpf) {
        iotrace_bpf__destroy(m_bpf);
        m_bpf = nullptr;
    }
}

void KernelTraceExecutor::initDeviceList(
        const std::vector<std::string> &devices) {
    for (auto const &dev : devices) {
        struct iotrace_event_device_desc dev_desc;
        std::string path(PATH_MAX, '\0');
        std::string basename;
        struct stat bStats;

        memset_s(&dev_desc, sizeof(dev_desc), 0);
        iotrace_event_init_hdr(&dev_desc.hdr, iotrace_event_type_device_desc, 0,
                               0, sizeof(dev_desc));

        if (::stat(dev.c_str(), &bStats)) {
            throw Exception("ERROR, cannot get device status of, " + dev);
        }

        if (S_ISLNK(bStats.st_mode)) {
            int result = readlink(dev.c_str(), &path[0], path.length() - 1);
            if (result <= 0) {
                throw Exception("ERROR, cannot resolve link of device " + dev);
            }

            log::verbose << "Resolve symbolic link from " << dev << "to "
                         << path << std::endl;
        } else {
            path = dev;
        }

        int fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            close(fd);
            throw Exception("ERROR, cannot open device, " + path);
        }

        if (::fstat(fd, &bStats)) {
            ::close(fd);
            throw Exception("ERROR, cannot get device status of, " + path);
        }

        if (!S_ISBLK(bStats.st_mode)) {
            ::close(fd);
            throw Exception("ERROR, expected block device, " + path);
        }

        // Use whole disk instead of partition if such defined
        char wholeDisk[sizeof(dev_desc.device_name)] = {0};
        dev_t wholeDiskId;
        if (blkid_devno_to_wholedisk(bStats.st_rdev, wholeDisk,
                                     sizeof(wholeDisk) - 1, &wholeDiskId)) {
            close(fd);
            throw Exception("ERROR, cannot get whole disk info");
        }
        if (wholeDiskId != bStats.st_rdev) {
            log::verbose << "Switch to whole disk, form " << path << " to /dev/"
                         << wholeDisk << std::endl;
        }

        // Get model name
        path = "/sys/block/" + std::string(wholeDisk) + "/device/model";
        std::ifstream iModel;
        iModel.open(path);
        if (iModel.good()) {
            std::string model;

            while (!iModel.eof()) {
                std::string n;
                iModel >> n;

                if (std::isprint(n[0])) {
                    if (model.length()) {
                        model += " ";
                    }
                    model += n;
                }
            }

            strcpy_s(dev_desc.device_model, sizeof(dev_desc.device_model) - 1,
                     model.c_str());
        }

        // Set device ID of whole deivce which will be traced
        dev_desc.id = MKDEV(major(wholeDiskId), minor(wholeDiskId));
        // Get device size
        dev_desc.device_size = blkid_get_dev_size(fd) >> 9;
        /* Copy user defined block device name */
        strcpy_s(dev_desc.device_name, sizeof(dev_desc.device_name), wholeDisk);

        log::cout << "Add device to trace, name: " << dev_desc.device_name
                  << ", id: " << dev_desc.id
                  << ", size: " << dev_desc.device_size << " sectors";
        if (dev_desc.device_model[0]) {
            log::cout << ", model: " << dev_desc.device_model;
        }
        log::cout << std::endl;

        close(fd);
        m_devList->push_back(dev_desc);
    }
}

}  // namespace octf

/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "KernelTraceExecutor.h"

#include <octf/interface/TraceConverter.h>
#include <octf/trace/iotrace_event.h>
#include <octf/utils/Exception.h>
#include <octf/utils/Log.h>
#include <octf/utils/SignalHandler.h>
#include <procfs_files.h>
#include <fstream>
#include <thread>
#include "KernelRingTraceProducer.h"

namespace octf {

KernelTraceExecutor::KernelTraceExecutor(
        const std::vector<std::string> &devices,
        uint32_t ringSizeMiB)
        : m_devices(devices)
        , m_startedDevices() {
    if (!isKernelModuleLoaded()) {
        throw Exception("Kernel tracing module is not loaded.");
    }

    // Check if kernel module version is compatible
    if (!checkModuleCompatibility()) {
        throw Exception("Kernel module version is incompatible.");
    }

    if (!writeSatraceProcfs(IOTRACE_PROCFS_SIZE_FILE_NAME,
                            std::to_string(ringSizeMiB))) {
        throw Exception("Failed to set ring buffer size \n");
    }

    for (const auto &dev : m_devices) {
        if (writeSatraceProcfs(IOTRACE_PROCFS_ADD_DEVICE_FILE_NAME, dev)) {
            m_startedDevices.push_back(dev);
            log::verbose << "Tracing started, device " << dev << std::endl;
        } else {
            stopDevices();
            throw Exception("Cannot start tracing, device " + dev);
        }
    }
}

bool KernelTraceExecutor::startTrace() {
    return true;
}

bool KernelTraceExecutor::stopTrace() {
    stopDevices();
    SignalHandler::get().sendSignal(SIGTERM);
    return true;
}

uint32_t KernelTraceExecutor::getTraceQueueCount() {
    return std::thread::hardware_concurrency();
}

std::unique_ptr<IRingTraceProducer> KernelTraceExecutor::createProducer(
        uint32_t queue) {
    return std::unique_ptr<IRingTraceProducer>(
            new KernelRingTraceProducer(queue));
}

std::unique_ptr<ITraceConverter> KernelTraceExecutor::createTraceConverter() {
    return std::unique_ptr<TraceConverter>(new TraceConverter());
}

bool KernelTraceExecutor::isKernelModuleLoaded() {
    std::string versionFilePath = std::string(IOTRACE_PROCFS_DIR) +
                                  "/" IOTRACE_PROCFS_VERSION_FILE_NAME;
    std::ifstream fileHandle(versionFilePath);

    if (!fileHandle.good()) {
        return false;
    }
    return true;
}

bool KernelTraceExecutor::checkModuleCompatibility() {
    std::string filePath = std::string(IOTRACE_PROCFS_DIR) + "/" +
                           IOTRACE_PROCFS_VERSION_FILE_NAME;

    std::fstream file;
    file.open(filePath, std::ios_base::in);

    if (file.fail()) {
        throw Exception("Failed to open kernel module version file: " +
                        filePath);
    }

    int major, minor;
    unsigned long long magic;
    file >> major;
    file >> minor;
    file >> std::hex >> magic;

    file.close();

    if (magic != IOTRACE_MAGIC || major != IOTRACE_EVENT_VERSION_MAJOR) {
        return false;
    }

    if (minor != IOTRACE_EVENT_VERSION_MINOR) {
        log::cout << "Minor version mismatch between kernel module and "
                     "current binary";
    }

    return true;
}

bool KernelTraceExecutor::writeSatraceProcfs(std::string file,
                                             const std::string &text) {
    std::string path = std::string{IOTRACE_PROCFS_DIR} + "/" + file;
    std::ofstream fd;

    fd.open(path, std::ios_base::out);
    if (fd.fail()) {
        return false;
    }

    fd << text << std::endl;

    if (fd.fail()) {
        fd.close();
        return false;
    }

    fd.close();

    return true;
}

void KernelTraceExecutor::waitUntilStopTrace() {
    // Register signal handler for SIGINT and SIGTERM
    SignalHandler::get().registerSignal(SIGINT);
    SignalHandler::get().registerSignal(SIGTERM);
    SignalHandler::get().wait();
}

void KernelTraceExecutor::stopDevices() {
    for (const auto &dev : m_startedDevices) {
        if (writeSatraceProcfs(IOTRACE_PROCFS_REMOVE_DEVICE_FILE_NAME, dev)) {
            log::verbose << "Tracing stopped, device " << dev << std::endl;
        } else {
            log::cerr << "Cannot stop tracing, device " << dev << std::endl;
        }
    }
    m_startedDevices.clear();
}

}  // namespace octf

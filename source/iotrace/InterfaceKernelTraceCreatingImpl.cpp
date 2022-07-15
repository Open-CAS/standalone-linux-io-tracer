/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "InterfaceKernelTraceCreatingImpl.h"

#include <sys/types.h>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <octf/interface/TraceManager.h>
#include <octf/plugin/NodePlugin.h>
#include <octf/proto/trace.pb.h>
#include <octf/trace/iotrace_event.h>
#include <octf/utils/Exception.h>
#include <octf/utils/Log.h>
#include "KernelTraceExecutor.h"

namespace octf {

InterfaceKernelTraceCreatingImpl::InterfaceKernelTraceCreatingImpl()
        : m_nodePath{NodeId("kernel")} {}

bool InterfaceKernelTraceCreatingImpl::checkIntegerParameters(
        const uint32_t value,
        const std::string &fieldName,
        const ::google::protobuf::Descriptor *messageDescriptor) {
    const auto field = messageDescriptor->FindFieldByLowercaseName(fieldName);
    if (field == nullptr) {
        throw Exception("Invalid parameter name");
    }

    const auto &valueInfo =
            field->options().GetExtension(proto::opts_param).cli_num();

    return (valueInfo.min() <= value) && (value <= valueInfo.max());
}

void InterfaceKernelTraceCreatingImpl::StartTracing(
        ::google::protobuf::RpcController *controller,
        const ::octf::proto::StartIoTraceRequest *request,
        ::octf::proto::TraceSummary *response,
        ::google::protobuf::Closure *done) {
    (void) response;
    try {
        std::map<std::string, std::string> tags;
        uint32_t maxDuration = request->maxduration();
        auto maxSize = request->maxsize();
        auto circBufferSize = request->circbuffersize();
        const auto &descriptor = request->descriptor();

        if (!checkIntegerParameters(maxDuration, "maxduration", descriptor)) {
            throw Exception("Invalid maximum trace duration");
        }
        if (!checkIntegerParameters(maxSize, "maxsize", descriptor)) {
            throw Exception("Invalid maximum trace size");
        }
        if (!checkIntegerParameters(circBufferSize, "circbuffersize",
                                    descriptor)) {
            throw Exception("Invalid circular buffer size");
        }
        /* Parse tags */
        for (const auto &tag : request->tag()) {
            parseTag(tag, tags);
        }

        probeModule();

        // List of devices to trace
        std::vector<std::string> devices(request->devicepaths_size());
        for (int i = 0; i < request->devicepaths_size(); i++) {
            devices[i] = request->devicepaths(i);
        }

        KernelTraceExecutor kernelExecutor(devices, circBufferSize);

        TraceManager manager(m_nodePath, &kernelExecutor);
        for (const auto &tag : tags) {
            manager.addTag(tag.first, tag.second);
        }

        manager.startJobs(maxDuration, maxSize, circBufferSize,
                          SerializerType::FileSerializer);

        kernelExecutor.waitUntilStopTrace();

        manager.stopJobs();

        TracingState state = manager.getState();
        manager.fillTraceSummary(response, state);

        if (state != TracingState::COMPLETE) {
            controller->SetFailed("Tracing not completed, trace path " +
                                  response->tracepath());
        }
    } catch (Exception &e) {
        controller->SetFailed(e.what());
    } catch (std::exception &e) {
        controller->SetFailed(e.what());
    }

    removeModule();
    done->Run();
}

auto static constexpr REMOVE_MODULE_COMMAND = "modprobe -r iotrace &>/dev/null";
auto static constexpr PROBE_MODULE_COMMAND = "modprobe iotrace &>/dev/null";

void InterfaceKernelTraceCreatingImpl::probeModule() {
    int result = std::system(REMOVE_MODULE_COMMAND);
    if (result) {
        throw Exception("Cannot reload iotrace kernel module");
    }

    // Make sure module is unloaded

    std::chrono::milliseconds timeout(1000);

    while (KernelTraceExecutor::isKernelModuleLoaded()) {
        std::chrono::milliseconds sleep(100);
        if (timeout > std::chrono::milliseconds(0)) {
            timeout -= sleep;
            std::this_thread::sleep_for(sleep);
        } else {
            throw Exception("Cannot close iotrace kernel module");
        }
    }

    result = std::system(PROBE_MODULE_COMMAND);
    if (result) {
        throw Exception("Cannot load iotrace kernel module");
    }

    // On some OSes, procfs' files provided by the module are not ready just
    // after module loading, thus wait a time until the module is fully up and
    // running
    timeout = std::chrono::milliseconds(1000);
    while (!KernelTraceExecutor::isKernelModuleLoaded()) {
        std::chrono::milliseconds sleep(100);
        if (timeout > std::chrono::milliseconds(0)) {
            timeout -= sleep;
            std::this_thread::sleep_for(sleep);
        } else {
            throw Exception("Cannot use iotrace kernel module");
        }
    }
}

void InterfaceKernelTraceCreatingImpl::removeModule() {
    int result = std::system(REMOVE_MODULE_COMMAND);
    if (result) {
        log::cerr << "Cannot remove iotrace kernel module" << std::endl;
    }
}

void InterfaceKernelTraceCreatingImpl::parseTag(
        const std::string &tag,
        std::map<std::string, std::string> &tags) {
    auto trimStrFn = [](std::string str) {
        const char *spaces = " \t\n\r\f\v";
        str.erase(0, str.find_first_not_of(spaces));
        str.erase(str.find_last_not_of(spaces) + 1);
        return str;
    };

    auto delimiter = tag.find('=');
    if (0 == delimiter) {
        throw Exception("Missing tag name");
    } else if (delimiter == tag.npos) {
        throw Exception("Missing tag value");
    } else {
        auto name = tag.substr(0, delimiter);
        auto len = tag.length() - delimiter - 1;
        auto value = tag.substr(delimiter + 1, len);

        name = trimStrFn(name);
        value = trimStrFn(value);

        if (!name.length()) {
            throw Exception("Tag name empty");
        }

        if (!value.length()) {
            throw Exception("Tag value empty");
        }

        tags[name] = value;
    }
}

}  // namespace octf

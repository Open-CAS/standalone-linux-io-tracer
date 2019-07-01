/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "InterfaceKernelTraceCreatingImpl.h"
#include <sys/types.h>
#include <cstdio>
#include <string>
#include <octf/interface/TraceManager.h>
#include <octf/plugin/NodePlugin.h>
#include <octf/proto/trace.pb.h>
#include <octf/trace/iotrace_event.h>
#include <octf/utils/Exception.h>
#include <octf/utils/SignalHandler.h>
#include "KernelTraceExecutor.h"

namespace octf {

InterfaceKernelTraceCreatingImpl::InterfaceKernelTraceCreatingImpl()
        : m_nodePath{NodeId("kernel")} {}

bool InterfaceKernelTraceCreatingImpl::checkIntegerParameters(
        const uint32_t value,
        const std::string &fieldName,
        const ::google::protobuf::Descriptor *messageDescriptor) {
    const auto field = messageDescriptor->FindFieldByLowercaseName(fieldName);
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

        // List of devices to trace
        std::vector<std::string> devices(request->devicepaths_size());
        for (int i = 0; i < request->devicepaths_size(); i++) {
            devices[i] = request->devicepaths(i);
        }

        KernelTraceExecutor kernelExecutor(devices, circBufferSize);

        TraceManager manager(m_nodePath, &kernelExecutor);

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
    }

    done->Run();
}

}  // namespace octf

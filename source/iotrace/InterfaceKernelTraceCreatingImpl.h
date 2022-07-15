/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SOURCE_USERSPACE_INTERFACEKERNELTRACECREATINGIMPL_H
#define SOURCE_USERSPACE_INTERFACEKERNELTRACECREATINGIMPL_H

#include <octf/interface/ITraceExecutor.h>
#include <octf/node/INode.h>
#include "InterfaceKernelTraceCreating.pb.h"

namespace octf {

/**
 * @brief Interface to allow tracing using kernel module
 */
class InterfaceKernelTraceCreatingImpl
        : public proto::InterfaceKernelTraceCreating {
public:
    /**
     * @param nodePath Path to owner node
     */
    InterfaceKernelTraceCreatingImpl();
    virtual ~InterfaceKernelTraceCreatingImpl() = default;

    virtual void StartTracing(::google::protobuf::RpcController *controller,
                              const ::octf::proto::StartIoTraceRequest *request,
                              ::octf::proto::TraceSummary *response,
                              ::google::protobuf::Closure *done);

private:
    bool checkIntegerParameters(
            const uint32_t value,
            const std::string &fieldName,
            const ::google::protobuf::Descriptor *messageDescriptor);

    void parseTag(const std::string &tag,
                  std::map<std::string, std::string> &tags);

private:
    const NodePath m_nodePath;
};

}  // namespace octf

#endif  // SOURCE_USERSPACE_INTERFACEKERNELTRACECREATINGIMPL_H

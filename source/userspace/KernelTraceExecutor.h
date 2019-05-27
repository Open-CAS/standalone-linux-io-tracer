/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_USERSPACE_KERNELTRACEEXECUTOR_H
#define SOURCE_USERSPACE_KERNELTRACEEXECUTOR_H

#include <string>
#include <vector>

#include <octf/interface/IRingTraceProducer.h>
#include <octf/interface/ITraceConverter.h>
#include <octf/interface/ITraceExecutor.h>

namespace octf {

/**
 * @brief Trace executor which allows tracing from kernel
 *
 * @note This executor sends SIGUSR1 to SignalHandler
 * to indicate end of tracing.
 */
class KernelTraceExecutor : public ITraceExecutor {
public:
    /**
     * @param devices Vector with paths of block devices to be traced
     */
    KernelTraceExecutor(const std::vector<std::string> &devices,
                        uint32_t circBufferSize);

    virtual ~KernelTraceExecutor() = default;

    bool startTrace() override;

    /**
     * @note This method sends SIGUSR1 to SignalHandler
     * to indicate end of tracing
     */
    bool stopTrace() override;

    uint32_t getTraceQueueCount() override;

    std::unique_ptr<IRingTraceProducer> createProducer(uint32_t queue) override;

    std::unique_ptr<ITraceConverter> createTraceConverter() override;

private:
    bool isKernelModuleLoaded();

    bool checkModuleCompatibility();

    void writeSatraceProcfs(std::string file, const std::string &text);

    std::vector<std::string> m_devices;
};

}  // namespace octf

#endif  // SOURCE_USERSPACE_KERNELTRACEEXECUTOR_H

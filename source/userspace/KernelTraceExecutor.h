/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SOURCE_USERSPACE_KERNELTRACEEXECUTOR_H
#define SOURCE_USERSPACE_KERNELTRACEEXECUTOR_H

#include <list>
#include <string>
#include <vector>
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

    bool stopTrace() override;

    uint32_t getTraceQueueCount() override;

    std::unique_ptr<IRingTraceProducer> createProducer(uint32_t queue) override;

    std::unique_ptr<ITraceConverter> createTraceConverter() override;

    /**
     * @brief Waits until receiving signal for stopping traces
     */
    void waitUntilStopTrace();

    /**
     * @brief Checks if IO tracer Linux kernel module is loaded
     *
     * @retval true module is loaded
     * @retval false module is not loaded
     */
    static bool isKernelModuleLoaded();

private:
    bool checkModuleCompatibility();

    bool writeSatraceProcfs(std::string file, const std::string &text);

    void stopDevices();

    std::vector<std::string> m_devices;
    std::list<std::string> m_startedDevices;
};

}  // namespace octf

#endif  // SOURCE_USERSPACE_KERNELTRACEEXECUTOR_H

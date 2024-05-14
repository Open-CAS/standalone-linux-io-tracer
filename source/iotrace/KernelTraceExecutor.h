/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SOURCE_USERSPACE_KERNELTRACEEXECUTOR_H
#define SOURCE_USERSPACE_KERNELTRACEEXECUTOR_H

#include <bpf/libbpf.h>
#include <stdint.h>
#include <list>
#include <string>
#include <thread>
#include <vector>
#include <octf/interface/ITraceExecutor.h>
#include <octf/trace/trace.h>
#include "KernelRingTraceProducer.h"

struct iotrace_bpf;
struct perf_buffer;

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

    virtual ~KernelTraceExecutor();

    bool startTrace() override;

    bool stopTrace() override;

    uint32_t getTraceQueueCount() override;

    std::unique_ptr<IRingTraceProducer> createProducer(uint32_t queue) override;

    std::unique_ptr<ITraceConverter> createTraceConverter() override;

    /**
     * @brief Waits until receiving signal for stopping traces
     */
    void waitUntilStopTrace();

private:
    static void perfEventHandler(void *ctx,
                                 int cpu,
                                 void *data,
                                 unsigned int data_sz);

    static void perfEventLost(void *ctx, int cpu, long long unsigned int lost);

    void destroyBpf();

    void initDeviceList(const std::vector<std::string> &devices);

    void initPerfBuffer();

private:
    const uint32_t m_traceQueueCount;
    struct iotrace_bpf *m_bpf;
    struct perf_buffer *m_bpfPerf;
    struct perf_buffer_opts m_bpfPerfBufOpts;
    std::thread m_bpfThread;
    std::vector<std::shared_ptr<KernelRingTraceBuffer>> m_traceProducerRings;
    KernelRingDevListShRef m_devList;
    KernelRingSeqIdShRef m_refSeqId;
    bool m_running;
};

}  // namespace octf

#endif  // SOURCE_USERSPACE_KERNELTRACEEXECUTOR_H

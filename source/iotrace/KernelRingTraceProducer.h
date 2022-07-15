/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SOURCE_USERSPACE_KERNELRINGTRACEPRODUCER_H
#define SOURCE_USERSPACE_KERNELRINGTRACEPRODUCER_H

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <vector>
#include <octf/interface/TraceProducerLocal.h>
#include <octf/trace/iotrace_event.h>
#include <octf/utils/NonCopyable.h>

namespace octf {

typedef std::list<struct iotrace_event_device_desc> KernelRingDevList;
typedef std::shared_ptr<KernelRingDevList> KernelRingDevListShRef;
typedef std::atomic<uint64_t> KernelRingSeqId;
typedef std::shared_ptr<KernelRingSeqId> KernelRingSeqIdShRef;

struct KernelRingTraceBuffer : public NonCopyable {
    KernelRingTraceBuffer()
            : devs()
            , refSeqId() {}
    virtual ~KernelRingTraceBuffer() {}

    KernelRingDevListShRef devs;
    KernelRingSeqIdShRef refSeqId;
    std::function<void(const void *trace, const uint32_t traceSize)> pushTrace;
    std::function<void(const uint64_t lost)> lostTrace;
};

/**
 * @brief Producer which utilizes ring buffer.
 *
 * This producer allows reading traces produced in kernel,
 * and utilizes procfs files. Because of this, pushTrace method
 * is not used.
 */
class KernelRingTraceProducer : public TraceProducerLocal {
public:
    KernelRingTraceProducer(std::shared_ptr<KernelRingTraceBuffer> traceBuffer,
                            int cpuId);
    ~KernelRingTraceProducer();

    void initRing(uint32_t memoryPoolSize) override;

    int getCpuAffinity(void) override;

private:
    std::shared_ptr<KernelRingTraceBuffer> m_traceBuffer;
    int m_cpuId;
};

}  // namespace octf

#endif  // SOURCE_USERSPACE_KERNELRINGTRACEPRODUCER_H

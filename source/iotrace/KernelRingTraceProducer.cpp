/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "KernelRingTraceProducer.h"

#include <octf/utils/Exception.h>

namespace octf {

KernelRingTraceProducer::KernelRingTraceProducer(
        std::shared_ptr<KernelRingTraceBuffer> traceBuffer,
        int cpuId)
        : TraceProducerLocal(cpuId)
        , m_traceBuffer(traceBuffer)
        , m_cpuId(cpuId) {
    traceBuffer->pushTrace = [this](const void *trace,
                                    const uint32_t traceSize) {
        pushTrace(trace, traceSize);
    };

    traceBuffer->lostTrace = [this](const uint64_t lost) {
        octf_trace_add_lost(getTraceProducerHandle(), lost);
    };
}

KernelRingTraceProducer::~KernelRingTraceProducer() {}

void KernelRingTraceProducer::initRing(uint32_t memoryPoolSize) {
    TraceProducerLocal::initRing(memoryPoolSize);

    auto hndl = getTraceProducerHandle();
    auto &refSeqId = *m_traceBuffer->refSeqId;

    // Push traced devices to the trace ring
    for (auto desc : *m_traceBuffer->devs) {
        desc.hdr.sid = ++refSeqId;

        auto result = octf_trace_push(hndl, &desc, sizeof(desc));
        if (result) {
            throw Exception("Cannot trace device description");
        }
    }
}

int KernelRingTraceProducer::getCpuAffinity(void) {
    return m_cpuId;
}

}  // namespace octf

/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_USERSPACE_KERNELRINGTRACEPRODUCER_H
#define SOURCE_USERSPACE_KERNELRINGTRACEPRODUCER_H

#include <atomic>
#include <memory>
#include <octf/interface/IRingTraceProducer.h>

namespace octf {

/**
 * @brief Producer which utilizes ring buffer.
 *
 * This producer allows reading traces produced in kernel,
 * and utilizes procfs files. Because of this, pushTrace method
 * is not used.
 */
class KernelRingTraceProducer : public IRingTraceProducer {
public:
    KernelRingTraceProducer(int cpuId);
    ~KernelRingTraceProducer();

    char *getBuffer(void) override;

    size_t getSize(void) const override;

    octf_trace_hdr_t *getConsumerHeader(void) override;

    bool wait(std::chrono::time_point<std::chrono::steady_clock> &endTime)
            override;

    void stop(void) override;

    void initRing(uint32_t memoryPoolSize) override;

    void deinitRing() override;

    int getCpuAffinity(void) override;

    int32_t getQueueId() override;

    /**
     * @note Because this producer utilizes procfs files to push traces
     * this method is not used.
     */
    int pushTrace(const void *trace, const uint32_t traceSize) override;

private:
    struct MappedFile {
        MappedFile(std::string path,
                   int open_flags,
                   int map_prot,
                   uint64_t max_size);
        ~MappedFile();

        char *buffer;
        int fd;
        size_t length;
    };

    std::unique_ptr<struct MappedFile> m_ring, m_consumer_hdr;

    std::atomic<bool> m_stopped;
    int m_cpuId;
};

}  // namespace octf

#endif  // SOURCE_USERSPACE_KERNELRINGTRACEPRODUCER_H

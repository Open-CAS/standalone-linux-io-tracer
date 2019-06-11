/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "KernelRingTraceProducer.h"
#include <fcntl.h>
#include <octf/trace/iotrace_event.h>
#include <octf/utils/Exception.h>
#include <procfs_files.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include "InterfaceKernelTraceCreatingImpl.h"

namespace octf {

KernelRingTraceProducer::MappedFile::MappedFile(std::string path,
                                                int open_flags,
                                                int map_prot,
                                                uint64_t max_size) {
    struct stat st;

    // Open file
    this->fd = open(path.c_str(), open_flags, 0);
    if (this->fd == -1) {
        throw Exception("Failed to open trace file: " + path);
    }

    // Verify size *after* openning - just to make sure noone changed it in the
    // meantime.
    if (fstat(this->fd, &st) != 0) {
        close(this->fd);
        throw Exception("Could not stat file: " + path);
    }
    if (st.st_size > max_size) {
        close(this->fd);
        throw Exception("Unexpected kernel buffer size");
    }
    this->length = st.st_size;

    // Map file
    this->buffer = static_cast<char *>(
            mmap(0, st.st_size, map_prot, MAP_SHARED, this->fd, 0));
    if (this->buffer == MAP_FAILED || this->buffer == NULL) {
        close(this->fd);
        throw Exception("Failed to map trace file: " + path);
    }
}

KernelRingTraceProducer::MappedFile::~MappedFile() {
    munmap(this->buffer, this->length);
    close(this->fd);
}

KernelRingTraceProducer::KernelRingTraceProducer(int cpuId)
        : m_stopped(false)
        , m_cpuId(cpuId) {}

KernelRingTraceProducer::~KernelRingTraceProducer() {
    deinitRing();
}

int32_t KernelRingTraceProducer::getQueueId() {
    return m_cpuId;
}

int KernelRingTraceProducer::pushTrace(const void __attribute__((__unused__)) *
                                               trace,
                                       const uint32_t
                                       __attribute__((__unused__)) traceSize) {
    throw Exception("pushTrace called on kernel producer");
    return -1;
}

char *KernelRingTraceProducer::getBuffer(void) {
    return m_ring->buffer;
}

size_t KernelRingTraceProducer::getSize(void) const {
    return m_ring->length;
}

octf_trace_hdr_t *KernelRingTraceProducer::getConsumerHeader(void) {
    return reinterpret_cast<octf_trace_hdr_t *>(m_consumer_hdr->buffer);
}

bool KernelRingTraceProducer::wait(
        std::chrono::time_point<std::chrono::steady_clock> &endTime) {
    fd_set readFileDescriptors;
    struct timeval tv;
    int result;

    while (!m_stopped) {
        if (std::chrono::steady_clock::now() >= endTime) {
            return false;
        }

        // Poll trace file for possible reads
        FD_ZERO(&readFileDescriptors);
        FD_SET(m_ring->fd, &readFileDescriptors);

        // Timeout - 0.5s
        tv.tv_sec = 0;
        tv.tv_usec = 500000;

        result = select(m_ring->fd + 1, &readFileDescriptors, nullptr, nullptr,
                        &tv);

        if (result < 0) {
            // Error
            throw Exception("select() failed with: " + std::to_string(result));

        } else if (result == 0) {
            // Timeout - no events, poll until producer is stopped
            continue;

        } else if (FD_ISSET(m_ring->fd, &readFileDescriptors)) {
            // Events are ready to be read
            return true;
        }
    }

    return false;
}

// force wait routine exit with false
void KernelRingTraceProducer::stop(void) {
    m_stopped = true;
}

void KernelRingTraceProducer::initRing(uint32_t memoryPoolSize) {
    std::string ring_file_path = std::string{IOTRACE_PROCFS_DIR} + "/" +
                                 IOTRACE_PROCFS_TRACE_FILE_PREFIX +
                                 std::to_string(m_cpuId);
    std::string consumer_hdr_file_path =
            std::string{IOTRACE_PROCFS_DIR} + "/" +
            IOTRACE_PROCFS_CONSUMER_HDR_FILE_PREFIX + std::to_string(m_cpuId);

    std::unique_ptr<struct MappedFile> ring(
            new KernelRingTraceProducer::MappedFile(ring_file_path, O_RDONLY,
                                                    PROT_READ, memoryPoolSize));

    std::unique_ptr<struct MappedFile> consumer_hdr(
            new KernelRingTraceProducer::MappedFile(
                    consumer_hdr_file_path, O_RDWR, PROT_READ | PROT_WRITE,
                    sizeof(octf_trace_hdr_t)));

    if (ring->length + consumer_hdr->length != memoryPoolSize) {
        throw Exception("Unexpected kernel circular buffer size");
    }

    m_ring = std::move(ring);
    m_consumer_hdr = std::move(consumer_hdr);
}

void KernelRingTraceProducer::deinitRing() {
    m_ring = NULL;
    m_consumer_hdr = NULL;
}

int KernelRingTraceProducer::getCpuAffinity(void) {
    return m_cpuId;
}

}  // namespace octf

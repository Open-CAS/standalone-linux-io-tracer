/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_KERNEL_INTERNAL_IO_TRACE_H
#define SOURCE_KERNEL_INTERNAL_IO_TRACE_H

#include <linux/mutex.h>
#include "trace.h"
#include "trace_inode.h"

struct iotrace_context;
struct iotrace_inode;

/**
 * @brief Global tracing state
 */
struct iotrace_state {
    /** iotrace per CPU objects */
    octf_trace_t __percpu *traces;

    /** iotrace per CPU objects */
    iotrace_inode_t __percpu *inode_traces;

    /** Sequential number */
    atomic64_t sid;

    /** Mutex for client attach / detach */
    struct mutex client_mutex;

    /* Number of attached clients */
    unsigned clients;
};

int iotrace_trace_init(struct iotrace_context *iotrace);

void iotrace_trace_deinit(struct iotrace_context *iotrace);

int iotrace_init_buffers(struct iotrace_context *iotrace, uint64_t size);

uint64_t iotrace_get_buffer_size(struct iotrace_context *iotrace);

int iotrace_trace_desc(struct iotrace_context *iotrace,
                       unsigned cpu,
                       uint32_t dev_id,
                       const char *dev_name,
                       uint64_t dev_size);

int iotrace_attach_client(struct iotrace_context *iotrace);

void iotrace_detach_client(struct iotrace_context *iotrace);

#endif  // SOURCE_KERNEL_INTERNAL_IO_TRACE_H

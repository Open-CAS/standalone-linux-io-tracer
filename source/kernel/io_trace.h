/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SOURCE_KERNEL_INTERNAL_IO_TRACE_H
#define SOURCE_KERNEL_INTERNAL_IO_TRACE_H

#include <linux/mutex.h>
#include "trace.h"
#include "trace_inode.h"

struct iotrace_context;
struct iotrace_inode_tracer;

/**
 * @brief Global tracing state
 */
struct iotrace_state {
    /** iotrace per CPU objects */
    octf_trace_t __percpu *traces;

    /** iotrace per CPU objects */
    iotrace_inode_tracer_t __percpu *inode_traces;

    /** Sequential number */
    atomic64_t sid;

    /* Number of attached clients */
    unsigned clients;
};

int iotrace_trace_init(struct iotrace_context *iotrace);

void iotrace_trace_deinit(struct iotrace_context *iotrace);

int iotrace_init_buffers(struct iotrace_context *iotrace, uint64_t size);

uint64_t iotrace_get_buffer_size(struct iotrace_context *iotrace);

int iotrace_trace_desc(struct iotrace_context *iotrace,
                       unsigned cpu,
                       uint64_t dev_id,
                       const char *dev_name,
                       const char *dev_model,
                       uint64_t dev_size);

int iotrace_attach_client(struct iotrace_context *iotrace);

void iotrace_detach_client(struct iotrace_context *iotrace);

#endif  // SOURCE_KERNEL_INTERNAL_IO_TRACE_H

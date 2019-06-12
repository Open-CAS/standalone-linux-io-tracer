/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_KERNEL_INTERNAL_CONTEXT_H
#define SOURCE_KERNEL_INTERNAL_CONTEXT_H

#include "io_trace.h"
#include "procfs.h"
#include "trace.h"
#include "trace_bdev.h"

/**
 * @brief Tracing global context
 */
struct iotrace_context {
    /** Traced block devices info */
    struct iotrace_bdev bdev;

    /** Procfs file per CPU descriptions */
    struct iotrace_proc_file __percpu *proc_files;

    /** Tracing state (seq no etc) */
    struct iotrace_state trace_state;

    /** Log buffer size */
    uint64_t size;
};

struct iotrace_context *iotrace_get_context(void);

#endif  // SOURCE_KERNEL_INTERNAL_CONTEXT_H

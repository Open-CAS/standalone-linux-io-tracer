/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SOURCE_KERNEL_INTERNAL_CONTEXT_H
#define SOURCE_KERNEL_INTERNAL_CONTEXT_H

#include <asm/atomic.h>
#include "io_trace.h"
#include "procfs.h"
#include "trace.h"
#include "trace_bdev.h"
#include "trace_env_kernel.h"
#include "trace_inode.h"

/**
 * Per CPU iotrace context
 */
struct iotrace_cpu_context {
    /** Is there a process waiting for traces flag */
    atomic_t waiting_for_trace;

    /** Wait queue to wake up waiting processes for traces */
    wait_queue_head_t wait_queue;

    /** Procfs file per CPU descriptions */
    struct iotrace_proc_file proc_files;
};

/**
 * @brief Tracing global context
 */
struct iotrace_context {
    /** Mutex for synchronization */
    struct mutex mutex;

    /** Traced block devices info */
    struct iotrace_bdev bdev;

    /** Tracing state (seq no etc) */
    struct iotrace_state trace_state;

    /** Per CPU context */
    struct iotrace_cpu_context __percpu *cpu_context;

    /** Log buffer size */
    uint64_t size;
};

struct iotrace_context *iotrace_get_context(void);

#endif  // SOURCE_KERNEL_INTERNAL_CONTEXT_H

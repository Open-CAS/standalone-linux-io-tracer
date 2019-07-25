/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_KERNEL_INTERNAL_PROCFS_H
#define SOURCE_KERNEL_INTERNAL_PROCFS_H

#include <linux/kref.h>
#include <linux/wait.h>
#include "trace.h"

struct iotrace_context;

struct iotrace_proc_file {
    int cpu;
    struct proc_dir_entry *trace_ring_entry, *consumer_hdr_entry;
    struct kref ref;
    void *trace_ring;
    octf_trace_hdr_t *consumer_hdr;
    uint64_t trace_ring_size;
    bool inited;
    wait_queue_head_t poll_wait_queue;
};

int iotrace_procfs_init(struct iotrace_context *iotrace);

void iotrace_procfs_deinit(struct iotrace_context *iotrace);

int iotrace_procfs_trace_file_alloc(struct iotrace_proc_file *proc_file,
                                    uint64_t size,
                                    int cpu);

#endif  // SOURCE_KERNEL_INTERNAL_PROCFS_H

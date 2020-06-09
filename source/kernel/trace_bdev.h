/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_KERNEL_INTERNAL_TRACE_BDEV_H
#define SOURCE_KERNEL_INTERNAL_TRACE_BDEV_H

#include <linux/blkdev.h>
#include <linux/genhd.h>
#include "procfs_files.h"

/**
 * @brief Traced devices info
 */
struct iotrace_bdev {
    /** Array of traced devices ptrs (per-cpu variable ) */
    struct block_device *__percpu *list;

    /** number of traced devices - only for use in  management path
     *  as different CPUs might have different number of bdevs in
     *  bdev_list while management operation is in progress */
    unsigned num;
};

/**
 * @brief Gets device with given queue, if it was added to trace list
 *
 * @usage This function is designed to be called with preemption disabled.
 *
 * @param trace_bdev iotrace device list
 * @param cpu running CPU
 * @param q request queue
 *
 * @return Block device associated with request queue
 * @retval NULL if block device isn't registered in iotracer
 * @retval block_device associated with request_queue if it was registered
 * in iotracer
 */
static inline struct block_device *iotrace_get_bdev_from_queue(
        struct iotrace_bdev *trace_bdev,
        unsigned cpu,
        struct request_queue *q) {
    struct block_device **bdev_list;
    unsigned i;

    bdev_list = per_cpu_ptr(trace_bdev->list, cpu);

    for (i = 0; i < IOTRACE_MAX_DEVICES && bdev_list[i] != NULL; i++) {
        if (bdev_list[i]->bd_queue == q)
            return bdev_list[i];
    }

    return NULL;
}

int iotrace_bdev_list(struct iotrace_bdev *trace_bdev,
                      char **list,
                      size_t list_len,
                      size_t entry_len);

int iotrace_bdev_remove(struct iotrace_bdev *trace_bdev, const char *path);

int iotrace_bdev_add(struct iotrace_bdev *trace_bdev, const char *path);

void iotrace_bdev_remove_all_locked(struct iotrace_bdev *trace_bdev);

int iotrace_bdev_init(struct iotrace_bdev *trace_bdev);

void iotrace_bdev_deinit(struct iotrace_bdev *trace_bdev);

#endif  // SOURCE_KERNEL_INTERNAL_TRACE_BDEV_H

/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include "context.h"
#include "io_trace.h"
#include "procfs.h"
#include "procfs_files.h"
#include "trace.h"
#include "trace_bdev.h"
#include "trace_inode.h"

MODULE_AUTHOR("Intel(R) Corporation");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(IOTRACE_VERSION_STRING);

/**
 * @brief Tracing global context
 */
static struct iotrace_context _sa = {}, *iotrace = &_sa;

struct iotrace_context *iotrace_get_context(void) {
    return iotrace;
}

/**
 * @brief Module initialization routine
 *
 * @retval 0 Module initialized successfully
 * @retval non-zero Error code
 */
static int __init iotrace_init_module(void) {
    int result = 0;

    result = iotrace_trace_init(iotrace);
    if (result)
        return result;

    result = iotrace_bdev_init(&iotrace->bdev);
    if (result)
        goto error_bdev_init;

    result = iotrace_procfs_init(iotrace);
    if (result)
        goto error_procfs_init;

    printk(KERN_INFO "iotrace module loaded, version %s\n",
           IOTRACE_VERSION_STRING);

    return 0;

error_procfs_init:
    iotrace_bdev_deinit(&iotrace->bdev);
error_bdev_init:
    iotrace_trace_deinit(iotrace);

    return result;
}

module_init(iotrace_init_module);

/**
 * @brief Module deinitialization routine
 */
static void __exit iotrace_exit_module(void) {
    /* remove procfs files */
    iotrace_procfs_deinit(iotrace);
    /* deinitialize devices list */
    iotrace_bdev_deinit(&iotrace->bdev);
    /* deinitialize tracing context */
    iotrace_trace_deinit(iotrace);

    printk(KERN_INFO "iotrace module unloaded, version %s\n",
           IOTRACE_VERSION_STRING);
}

module_exit(iotrace_exit_module);

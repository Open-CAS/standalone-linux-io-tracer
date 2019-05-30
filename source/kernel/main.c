/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include "procfs.h"
#include "io_trace.h"
#include "trace_bdev.h"
#include "context.h"
#include "trace.h"

#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)

MODULE_AUTHOR("Intel(R) Corporation");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(VALUE(IOTRACE_VERSION));

/**
 * @brief Tracing global context
 */
static struct iotrace_context _sa = {}, *iotrace = &_sa;

struct iotrace_context *iotrace_get_context(void)
{
	return iotrace;
}

/**
 * @brief Module initialization routine
 *
 * @retval 0 Module initialized successfully
 * @retval non-zero Error code
 */
static int __init iotrace_init_module(void)
{
	int result = 0;

	if (iotrace_trace_init(iotrace))
		goto free_context;

	result = iotrace_bdev_init(&iotrace->bdev);
	if (result)
		goto free_context;

	result = iotrace_procfs_init(iotrace);
	if (result)
		goto free_sa;

	if (VALUE(IOTRACE_VERSION_LABEL)[0] != '\0')
		printk(KERN_INFO "Module loaded successfully: iotrace version %s (%s)\n", VALUE(IOTRACE_VERSION), VALUE(IOTRACE_VERSION_LABEL));
	else
		printk(KERN_INFO "Module loaded successfully: iotrace version %s\n", VALUE(IOTRACE_VERSION));

	return 0;

free_sa:
	iotrace_bdev_deinit(&iotrace->bdev);
free_context:
	iotrace_trace_deinit(iotrace);
	return result;
}

module_init(iotrace_init_module);

/**
 * @brief Module deinitialization routine
 */
static void __exit iotrace_exit_module(void)
{
	/* remove procfs files */
	iotrace_procfs_deinit(iotrace);
	/* deinitialize devices list */
	iotrace_bdev_deinit(&iotrace->bdev);
	/* deinitialize tracing context */
	iotrace_trace_deinit(iotrace);
}

module_exit(iotrace_exit_module);

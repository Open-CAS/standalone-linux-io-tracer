/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_INCLUDES_PROCFS_FILES_H
#define SOURCE_INCLUDES_PROCFS_FILES_H

#include <linux/ioctl.h>

#define IOTRACE_PROCFS_DIR "/proc/iotrace/"
#define IOTRACE_PROCFS_SUBDIR_NAME "iotrace"

#define IOTRACE_PROCFS_ADD_DEVICE_FILE_NAME "add_device"

#define IOTRACE_PROCFS_REMOVE_DEVICE_FILE_NAME "remove_device"

#define IOTRACE_PROCFS_TRACE_FILE_PREFIX "trace_ring."

#define IOTRACE_PROCFS_CONSUMER_HDR_FILE_PREFIX "consumer_hdr."

#define IOTRACE_PROCFS_DEVICES_FILE_NAME "devices"

#define IOTRACE_PROCFS_VERSION_FILE_NAME "version"

#define IOTRACE_PROCFS_SIZE_FILE_NAME "size"

static const uint64_t iotrace_procfs_max_buffer_size_mb =
        4096; /** 4GiB max for all cpus */

#define IOTRACE_IOCTL_MAGIC 0x77
#define IOTRACE_IOCTL_WAIT_FOR_TRACES _IO(IOTRACE_IOCTL_MAGIC, 1)
#define IOTRACE_IOCTL_INTERRUPT_WAIT_FOR_TRACES _IO(IOTRACE_IOCTL_MAGIC, 2)

#endif  // SOURCE_INCLUDES_PROCFS_FILES_H

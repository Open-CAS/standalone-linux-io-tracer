/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "trace_bdev.h"
#include <linux/blkdev.h>
#include <linux/cred.h>
#include <linux/ctype.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/smp.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include "config.h"
#include "context.h"
#include "io_trace.h"

/**
 * @brief Helper structure to aggregate parameters to for_each_cpu callback
 *	used to add/remove device from list.
 */
struct iotrace_bdev_data {
    /** iotrace traced devices info */
    struct iotrace_bdev *trace_bdev;

    /** block device to be added */
    struct block_device *bdev;
    /** index at which to delete a device */
    unsigned idx;

    /** Device model */
    char bdev_model[128];
};

/**
 * @brief Add block device pointer to per-cpu @trace_bdev array
 *
 * @usage This function is designed to be called using on_each_cpu macro,
 *     pinned to fixed CPU in order to ensure that trace_bdev->list is not
 *     modified concurrently. Also management lock should be held by
 *     the caller to avoid re-entrance in management path.
 *
 * @param info Input data structure (iotrace device list and bdev)
 */
void static iotrace_bdev_add_oncpu(void *info) {
    struct iotrace_bdev_data *data = info;
    struct iotrace_bdev *trace_bdev = data->trace_bdev;
    unsigned cpu = get_cpu();
    struct gendisk *gd = data->bdev->bd_disk;
    struct iotrace_context *iotrace = iotrace_get_context();
    uint64_t bdev_size = 0;
    bdev_size = (data->bdev->bd_contains == data->bdev)
                        ? get_capacity(data->bdev->bd_disk)
                        : data->bdev->bd_part->nr_sects;

    BUG_ON(trace_bdev->num >= IOTRACE_MAX_DEVICES);
    per_cpu_ptr(trace_bdev->list, cpu)[trace_bdev->num] = data->bdev;

    iotrace_trace_desc(iotrace, cpu, disk_devt(gd), gd->disk_name,
                       data->bdev_model, bdev_size);
    put_cpu();
}

static void iotrace_bdev_get_model(struct iotrace_bdev_data *data) {
    struct gendisk *gd = data->bdev->bd_disk;
    char filename[256] = {'\0'};
    struct path path;
    struct file *file;
    loff_t off = 0;
    int i;

    snprintf(filename, sizeof(filename) - 1, "/sys/block/%s/device/model",
             gd->disk_name);

    if (kern_path(filename, LOOKUP_FOLLOW, &path)) {
        return;
    }

    file = dentry_open(&path, O_RDONLY, current_cred());
    if (IS_ERR(file)) {
        path_put(&path);
        return;
    }

    iotrace_kernel_read(file, data->bdev_model, sizeof(data->bdev_model) - 1,
                        &off);
    fput(file);
    path_put(&path);

    // Trim white spaces at the end of string
    for (i = sizeof(data->bdev_model) - 1; i > 0; i--) {
        char ch = data->bdev_model[i];

        if ('\0' == ch || isspace(ch)) {
            ch = '\0';
        } else {
            break;
        }

        data->bdev_model[i] = ch;
    }
}

/**
 * @brief Add device to trace list
 *
 * @param trace_bdev iotrace block device list
 * @param path device path
 *
 * @retval 0 device added successfully
 * @retval non-zero Error code
 */
int iotrace_bdev_add(struct iotrace_bdev *trace_bdev, const char *path) {
    struct block_device *bdev;
    struct iotrace_bdev_data data = {.trace_bdev = trace_bdev};
    struct block_device **bdev_list;
    struct iotrace_context *context = iotrace_get_context();
    int result;
    unsigned i;
    unsigned cpu;

    if (strnlen(path, PATH_MAX) >= PATH_MAX) {
        printk(KERN_ERR "Path too long\n");
        result = -EINVAL;
        goto exit;
    }

    mutex_lock(&context->mutex);

    if (trace_bdev->num == IOTRACE_MAX_DEVICES) {
        result = -ENOSPC;
        goto unlock;
    }

    bdev = blkdev_get_by_path(path, FMODE_READ, trace_bdev);
    if (IS_ERR(bdev)) {
        result = (int) PTR_ERR(bdev);
        printk(KERN_ERR "Error looking up block device: %d\n", result);
        goto unlock;
    }

    /**
     * Get per-cpu variables. CPU can potentially change during execution
     * of this function, but it is ok - all CPUs should have identical copies
     * at this point */
    cpu = get_cpu();
    bdev_list = per_cpu_ptr(trace_bdev->list, cpu);
    /* Check if this queue is traced already */
    for (i = 0; i < trace_bdev->num; i++) {
        if (bdev_list[i]->bd_queue == bdev->bd_queue) {
            put_cpu();
            printk(KERN_ERR "Device already traced\n");
            result = -EPERM;
            goto put;
        }
    }
    put_cpu();
    /**
     * Add bdev to per-cpu array, actually running code on each CPU in order
     * to synchronize with I/O
     */
    data.bdev = bdev;
    iotrace_bdev_get_model(&data);

    if (iotrace_get_context()->trace_state.clients) {
        on_each_cpu(iotrace_bdev_add_oncpu, &data, true);
        trace_bdev->num++;
    } else {
        result = -EINVAL;
        goto put;
    }

    mutex_unlock(&context->mutex);

    return 0;

put:
    blkdev_put(bdev, FMODE_READ);
unlock:
    mutex_unlock(&context->mutex);
exit:
    return result;
}

/**
 * @brief Remove block device pointer from per-cpu @trace_bdev array
 *
 * @usage This function is designed to be called using on_each_cpu macro,
 *	pinned to fixed CPU in order to ensure that trace_bdev->list is not
 *	modified concurrently. Also management lock should be held by
 *	the caller to avoid re-entrance in management path.
 *
 * @param info Input data structure (iotrace device list and index to remove
 *bdev at)
 *
 */
void static iotrace_bdev_remove_oncpu(void *info) {
    struct iotrace_bdev_data *data = info;
    struct iotrace_bdev *trace_bdev = data->trace_bdev;
    unsigned cpu = get_cpu();
    struct block_device **bdev_list = per_cpu_ptr(trace_bdev->list, cpu);

    BUG_ON(trace_bdev->num == 0);
    bdev_list[data->idx] = bdev_list[trace_bdev->num - 1];
    bdev_list[trace_bdev->num - 1] = NULL;
    put_cpu();
}

/**
 * @brief Remove block device pointer from per-cpu @trace_bdev array
 *
 * @usage Caller must hold block device list lock (trace_bdev->lock)
 *
 * @param trace_bdev iotrace block device list
 * @param bdev block device to be removed from list
 *
 * @retval 0 device added successfully
 * @retval non-zero error code
 */
static int iotrace_bdev_remove_locked(struct iotrace_bdev *trace_bdev,
                                      struct block_device *bdev) {
    struct iotrace_bdev_data data = {.trace_bdev = trace_bdev};
    unsigned cpu = get_cpu();
    struct block_device **bdev_list;
    int result;
    unsigned i;
    /**
     * Get per-cpu variables. CPU can potentially change during execution
     * of this function, but it is ok - all CPUs should have identical copies
     * at this point */
    bdev_list = per_cpu_ptr(trace_bdev->list, cpu);
    result = -ENOENT;
    for (i = 0; i < trace_bdev->num; i++) {
        if (bdev_list[i] == bdev) {
            result = 0;
            break;
        }
    }
    put_cpu();

    if (result)
        goto exit;

    data.idx = i;
    on_each_cpu(iotrace_bdev_remove_oncpu, &data, true);

    trace_bdev->num--;

    blkdev_put(bdev, FMODE_READ);

exit:
    return result;
}

/**
 * @brief remove device from trace list
 *
 * @param trace_bdev iotrace block device list
 * @param path device path
 *
 * @retval 0 device added successfully
 * @retval non-zero error code
 */
int iotrace_bdev_remove(struct iotrace_bdev *trace_bdev, const char *path) {
    struct block_device *bdev;
    int result;

    if (strnlen(path, PATH_MAX) >= PATH_MAX) {
        printk(KERN_ERR "Path too long\n");
        result = -EINVAL;
        goto error;
    }

    bdev = IOTRACE_LOOKUP_BDEV(path);
    if (IS_ERR(bdev)) {
        result = PTR_ERR(bdev);
        goto error;
    }

    mutex_lock(&iotrace_get_context()->mutex);
    result = iotrace_bdev_remove_locked(trace_bdev, bdev);
    mutex_unlock(&iotrace_get_context()->mutex);

    bdput(bdev);
error:
    return result;
}

/**
 * @brief Remove all devices from trace list
 *
 * @usage Caller must hold block device list lock (trace_bdev->lock)
 *
 * @param trace_bdev iotrace block device list
 */
void iotrace_bdev_remove_all_locked(struct iotrace_bdev *trace_bdev) {
    struct block_device **bdev_list;
    unsigned cpu = get_cpu();
    bdev_list = per_cpu_ptr(trace_bdev->list, cpu);
    while (trace_bdev->num)
        iotrace_bdev_remove_locked(trace_bdev, bdev_list[trace_bdev->num - 1]);
    put_cpu();
}

/**
 * @brief Get list of all traced device names
 *
 * @param trace_bdev iotrace block device list
 * @param[out] list array of device name strings
 * @param list_len number of list entries
 * @param entry_len size of single entry
 *
 * @retval >=0 number of devices
 * @retval <0 error code (negation)
 */
int iotrace_bdev_list(struct iotrace_bdev *trace_bdev,
                      char **list,
                      size_t list_len,
                      size_t entry_len) {
    unsigned i;
    size_t len;
    const char *name;
    struct block_device **bdev_list;
    unsigned cpu;
    int num;

    mutex_lock(&iotrace_get_context()->mutex);
    cpu = get_cpu();
    bdev_list = per_cpu_ptr(trace_bdev->list, cpu);
    for (i = 0; i < trace_bdev->num; i++) {
        name = bdev_list[i]->bd_disk->disk_name;
        len = strnlen(name, DISK_NAME_LEN);
        if (len >= DISK_NAME_LEN) {
            mutex_unlock(&iotrace_get_context()->mutex);
            put_cpu();
            return -ENOSPC;
        }
        strlcpy(list[i], name, entry_len);
    }
    put_cpu();
    num = trace_bdev->num;

    mutex_unlock(&iotrace_get_context()->mutex);

    return num;
}

/**
 * @brief Initialize bdev list
 *
 * @param trace_bdev iotrace block device list
 *
 * @retval 0 Context initialized successfully
 * @retval non-zero Error code
 */
int iotrace_bdev_init(struct iotrace_bdev *trace_bdev) {
    const size_t bdev_list_size =
            sizeof(struct block_device *) * IOTRACE_MAX_DEVICES;

    trace_bdev->list = __alloc_percpu(bdev_list_size, 128);
    if (!trace_bdev->list)
        goto error;

    return 0;

error:
    return -ENOMEM;
}

/**
 * @brief Deinitializde bdev list
 *
 * @param trace_bdev iotrace block device list
 */
void iotrace_bdev_deinit(struct iotrace_bdev *trace_bdev) {
    free_percpu(trace_bdev->list);
}

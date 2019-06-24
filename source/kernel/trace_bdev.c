/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/smp.h>
#include <linux/blkdev.h>
#include <linux/version.h>
#include "trace_bdev.h"
#include "io_trace.h"
#include "context.h"

/* Include code specific to kernel version */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
#include "config-ubuntu.h"
#else
#include "config-epel.h"
#endif

/**
 * @brief Helper structure to aggregate parameters to for_each_cpu callback
 *	used to add/remove device from list.
 */
struct iotrace_bdev_data {
	/** iotrace traced devices info */
	struct iotrace_bdev *trace_bdev;
	union {
		/** block device to be added */
		struct block_device *bdev;
		/** index at which to delete a device */
		unsigned idx;
	};
};

/**
 * @brief Add block device pointer to per-cpu @trace_bdev array
 *
 * @usage This function is designed to be called using on_each_cpu macro,
 *	pinned to fixed CPU in order to ensure that trace_bdev->list is not
 *	modified concurrently. Also management lock should be held by
 *	the caller to avoid re-entrance in management path.
 *
 * @param info Input data structure (iotrace device list and bdev)
 */
void static iotrace_bdev_add_oncpu(void *info)
{
	struct iotrace_bdev_data *data = info;
	struct iotrace_bdev *trace_bdev = data->trace_bdev;
	unsigned cpu = smp_processor_id();
	struct gendisk *gd = data->bdev->bd_disk;
	struct iotrace_context *iotrace = iotrace_get_context();
	uint64_t bdev_size = 0;

	bdev_size = (data->bdev->bd_contains == data->bdev) ?
				get_capacity(data->bdev->bd_disk) :
				data->bdev->bd_part->nr_sects;

	BUG_ON(trace_bdev->num >= SATRACE_MAX_DEVICES);
	per_cpu_ptr(trace_bdev->list, cpu)[trace_bdev->num] = data->bdev;

	iotrace_trace_desc(iotrace, cpu, disk_devt(gd), gd->disk_name, bdev_size);
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
int iotrace_bdev_add(struct iotrace_bdev *trace_bdev, const char *path)
{
	struct block_device *bdev;
	struct iotrace_bdev_data data = { .trace_bdev = trace_bdev };
	struct block_device **bdev_list;
	int result;
	unsigned i;

	if (strnlen(path, PATH_MAX) >= PATH_MAX) {
		printk(KERN_ERR "Path too long\n");
		result = -EINVAL;
		goto exit;
	}

	mutex_lock(&trace_bdev->lock);

	if (trace_bdev->num == SATRACE_MAX_DEVICES) {
		result = -ENOSPC;
		goto unlock;
	}

	bdev = blkdev_get_by_path(path, FMODE_READ, trace_bdev);
	if (IS_ERR(bdev)) {
		result = (int)PTR_ERR(bdev);
		printk(KERN_ERR "Error looking up block device: %d\n", result);
		goto unlock;
	}

	/**
	 * Get per-cpu variables. CPU can potentially change during execution
	 * of this function, but it is ok - all CPUs should have identical copies
	 * at this point */
	bdev_list = per_cpu_ptr(trace_bdev->list, smp_processor_id());

	/* Check if this queue is traced already */
	for (i = 0; i < trace_bdev->num; i++) {
		if (bdev_list[i]->bd_queue == bdev->bd_queue) {
			printk(KERN_ERR "Device already traced\n");
			result = -EPERM;
			goto put;
		}
	}

	/**
	 * Add bdev to per-cpu array, actually running code on each CPU in order
	 * to synchronize with I/O
	 */
	data.bdev = bdev;

	/**
	 * Because adding device will push device description event into trace log,
	 * we demand all trace log structures to be initialized, thus we acquire
	 * lock responsible for protecting this resource.
	 */
	mutex_lock(&iotrace_get_context()->trace_state.client_mutex);
	on_each_cpu(iotrace_bdev_add_oncpu, &data, true);
	mutex_unlock(&iotrace_get_context()->trace_state.client_mutex);

	trace_bdev->num++;

	mutex_unlock(&trace_bdev->lock);

	return 0;

put:
	blkdev_put(bdev, FMODE_READ);
unlock:
	mutex_unlock(&trace_bdev->lock);
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
 * @param info Input data structure (iotrace device list and index to remove bdev
 *	at)
 *
 */
void static iotrace_bdev_remove_oncpu(void *info)
{
	struct iotrace_bdev_data *data = info;
	struct iotrace_bdev *trace_bdev = data->trace_bdev;
	unsigned cpu = smp_processor_id();
	struct block_device **bdev_list = per_cpu_ptr(trace_bdev->list, cpu);

	BUG_ON(trace_bdev->num == 0);
	bdev_list[data->idx] = bdev_list[trace_bdev->num - 1];
	bdev_list[trace_bdev->num - 1] = NULL;
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
					  struct block_device *bdev)
{
	struct iotrace_bdev_data data = { .trace_bdev = trace_bdev };
	unsigned cpu = smp_processor_id();
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
int iotrace_bdev_remove(struct iotrace_bdev *trace_bdev, const char *path)
{
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

	mutex_lock(&trace_bdev->lock);

	result = iotrace_bdev_remove_locked(trace_bdev, bdev);

	mutex_unlock(&trace_bdev->lock);

	bdput(bdev);
error:
	return result;
}

/**
 * @brief Remove all devices from trace list
 *
 * @param trace_bdev iotrace block device list
 */
void iotrace_bdev_remove_all(struct iotrace_bdev *trace_bdev)
{
	struct block_device **bdev_list;

	bdev_list = per_cpu_ptr(trace_bdev->list, smp_processor_id());

	mutex_lock(&trace_bdev->lock);

	while (trace_bdev->num)
		iotrace_bdev_remove_locked(trace_bdev, bdev_list[trace_bdev->num - 1]);

	mutex_unlock(&trace_bdev->lock);
}

/**
 * @brief Get list of all traced device names
 *
 * @param trace_bdev iotrace block device list
 * @param[out] list array of device name strings
 *
 * @retval >=0 number of devices
 * @retval <0 error code (negation)
 */
int iotrace_bdev_list(struct iotrace_bdev *trace_bdev,
			  char list[SATRACE_MAX_DEVICES][DISK_NAME_LEN])
{
	unsigned i;
	size_t len;
	const char *name;
	struct block_device **bdev_list;
	int num;

	mutex_lock(&trace_bdev->lock);

	bdev_list = per_cpu_ptr(trace_bdev->list, smp_processor_id());
	for (i = 0; i < trace_bdev->num; i++) {
		name = bdev_list[i]->bd_disk->disk_name;
		len = strnlen(name, DISK_NAME_LEN);
		if (len >= DISK_NAME_LEN) {
			mutex_unlock(&trace_bdev->lock);
			return -ENOSPC;
		}
		strlcpy(list[i], name, sizeof(list[i]));
	}
	num = trace_bdev->num;

	mutex_unlock(&trace_bdev->lock);

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
int iotrace_bdev_init(struct iotrace_bdev *trace_bdev)
{
	const size_t bdev_list_size =
		sizeof(struct block_device *) * SATRACE_MAX_DEVICES;

	trace_bdev->list = __alloc_percpu(bdev_list_size, 128);
	if (!trace_bdev->list)
		goto error;

	mutex_init(&trace_bdev->lock);

	return 0;

error:
	return -ENOMEM;
}

/**
 * @brief Deinitializde bdev list
 *
 * @param trace_bdev iotrace block device list
 */
void iotrace_bdev_deinit(struct iotrace_bdev *trace_bdev)
{
	free_percpu(trace_bdev->list);
}

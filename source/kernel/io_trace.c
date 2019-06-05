/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/types.h>
#include <linux/wait.h>
#include "trace_bio.h"
#include <linux/tracepoint.h>
#include <trace/events/block.h>
#include "io_trace.h"
#include "context.h"
#include "bio.h"
#include "procfs.h"
#include "iotrace_event.h"
#include "trace.h"
#include "procfs_files.h"

/* Maximal length of buffer with version information */
#define VERSION_BUFFER_MAX_LEN 64

static inline void iotrace_notify_of_new_events(struct iotrace_context *context,
						unsigned int cpu)
{
	wait_queue_head_t *queue =
		&(per_cpu_ptr(context->proc_files, cpu)->poll_wait_queue);

	wake_up(queue);
}

/**
 * @brief Write device information to trace buffer
 *
 * @param iotrace iotrace main context
 * @param cpu CPU id
 * @param dev_id Device id
 * @param dev_name device canonical name
 *
 * @retval 0 Information stored successfully in trace buffer
 * @retval non-zero Error code
 */
int iotrace_trace_desc(struct iotrace_context *iotrace, unsigned cpu,
		       uint32_t dev_id, const char *dev_name, uint64_t dev_size)
{
	int result = 0;
	struct iotrace_state *state = &iotrace->trace_state;
	octf_trace_t trace = *per_cpu_ptr(state->traces, cpu);

	struct iotrace_event_device_desc desc;
	uint64_t sid = atomic64_inc_return(&state->sid);
	const size_t dev_name_size = sizeof(desc.device_name);

	if (strnlen(dev_name, dev_name_size) >= dev_name_size)
		return -ENOSPC;
	strlcpy(desc.device_name, dev_name, sizeof(desc.device_name));

	iotrace_event_init_hdr(&desc.hdr, iotrace_event_type_device_desc, sid,
			  ktime_to_ns(ktime_get()), sizeof(desc));

	desc.id = dev_id;
	desc.device_size = dev_size;

	result = octf_trace_push(trace, &desc, sizeof(desc));

	iotrace_notify_of_new_events(iotrace, cpu);

	return result;
}

/**
 * @brief Function registered to be called each time BIO is queued
 *
 * @param ignore Ignore this param
 * @param q Associated request queue
 * @param bio I/O description
 *
 */
static void bio_queue_event(void *ignore, struct request_queue *q,
			    struct bio *bio)
{
	uint32_t dev_id;
	unsigned cpu = get_cpu();
	struct iotrace_context *iotrace = iotrace_get_context();

	if (iotrace_bdev_is_added(&iotrace->bdev, cpu, q)) {
		dev_id = disk_devt(bio->bi_bdev->bd_disk);
		iotrace_trace_bio(iotrace, cpu, dev_id, bio);
		iotrace_notify_of_new_events(iotrace, cpu);
	}

	put_cpu();

	return;
}

/**
 * @brief Deinitialize iotrace tracers
 *
 * Close all iotrace objects
 *
 * @param state iotrace state
 *
 */
static void deinit_tracers(struct iotrace_state *state)
{
	unsigned i;

	for_each_online_cpu (i)
		octf_trace_close(per_cpu_ptr(state->traces, i));

	free_percpu(state->traces);
}

/**
 * @brief Initialize iotrace tracers
 *
 * Open iotrace objects (one per each procfs file)
 *
 * @param state iotrace state
 * @param proc_files proc files description
 *
 * @retval 0 Tracers initialized successfully
 * @retval non-zero Error code
 */
static int init_tracers(struct iotrace_state *state,
			struct iotrace_proc_file __percpu *proc_files)
{
	int result = -EINVAL;
	unsigned i;
	octf_trace_t *trace;
	struct iotrace_proc_file *file;

	state->traces = alloc_percpu(octf_trace_t);
	if (!state->traces)
		return -ENOMEM;

	for_each_online_cpu (i) {
		trace = per_cpu_ptr(state->traces, i);
		file = per_cpu_ptr(proc_files, i);

		if (!file->trace_ring) {
			printk(KERN_ERR "Trace buffer is not allocated\n");
			result = -EINVAL;
			break;
		}

		result =
			octf_trace_open(file->trace_ring, file->trace_ring_size,
					file->consumer_hdr,
					octf_trace_open_mode_producer, trace);
		if (result)
			break;
	}

	if (result) {
		for_each_online_cpu (i) {
			trace = per_cpu_ptr(state->traces, i);
			octf_trace_close(trace);
		}
		free_percpu(state->traces);
		return result;
	}

	return 0;
}

static int iotrace_set_buffer_size(struct iotrace_context *iotrace, uint64_t size_mb)
{
	unsigned long long size = size_mb * 1024ULL * 1024ULL /
				  (unsigned long long)num_online_cpus();

	if (size_mb > iotrace_procfs_max_buffer_size_mb || size == 0)
		return -EINVAL;

	iotrace->size = size;

	return 0;
}

/**
 * @brief Get total trace buffer size for all CPUs, in MiB
 *
 * @param iotrace iotrace context
 *
 * @return buffer size
 */
uint64_t iotrace_get_buffer_size(struct iotrace_context *iotrace)
{
	return iotrace_get_context()->size * num_online_cpus() / 1024ULL /
	       1024ULL;
}

/**
 * @brief Initialize trace buffers of given size
 *
 * @param iotrace iotrace context
 * @param size Total trace buffer size for all CPUs, in MiB
 *
 * @retval 0 Buffers initialized successfully
 * @retval non-zero Error code
 */
int iotrace_init_buffers(struct iotrace_context *iotrace, uint64_t size)
{
	struct iotrace_state *state = &iotrace->trace_state;
	struct iotrace_proc_file __percpu *proc_files = iotrace->proc_files;
	struct iotrace_proc_file *file;
	int result = 0;
	int i;

	mutex_lock(&state->client_mutex);

	if (state->clients) {
		result = -EINVAL;
		goto exit;
	}

	result = iotrace_set_buffer_size(iotrace, size);
	if (result)
		goto exit;

	for_each_online_cpu (i) {
		file = per_cpu_ptr(proc_files, i);

		result = iotrace_procfs_trace_file_alloc(file, iotrace->size);
		if (result)
			break;
	}

exit:
	mutex_unlock(&state->client_mutex);
	return result;
}

/**
 * @brief Initialize tracing for new consumer
 *
 * @param iotrace main iotrace context
 *
 */
int iotrace_attach_client(struct iotrace_context *iotrace)
{
	struct iotrace_state *state = &iotrace->trace_state;
	struct iotrace_proc_file __percpu *proc_files = iotrace->proc_files;
	int result = 0;

	mutex_lock(&state->client_mutex);

	if (!state->clients) {
		result = init_tracers(state, proc_files);
		if (result)
			goto exit;

		result = register_trace_block_bio_queue(bio_queue_event, NULL);
		if (result) {
			printk(KERN_ERR "Failed to register trace probe: %d\n",
			       result);
			deinit_tracers(state);
			goto exit;
		}
		printk(KERN_INFO "Registered tracing callback\n");
	}

	++state->clients;

exit:
	mutex_unlock(&state->client_mutex);
	return result;
}

/**
 * @brief Deinitialize tracing after last buffer handle is closed
 *
 * @param iotrace main iotrace context
 *
 */
void iotrace_detach_client(struct iotrace_context *iotrace)
{
	struct iotrace_state *state = &iotrace->trace_state;
	mutex_lock(&state->client_mutex);

	state->clients--;
	if (state->clients) {
		mutex_unlock(&state->client_mutex);
		return;
	}

	/* unregister callback */
	unregister_trace_block_bio_queue(bio_queue_event, NULL);
	printk(KERN_INFO "Unregistered tracing callback\n");

	/* remove all devices from trace list */
	iotrace_bdev_remove_all(&iotrace_get_context()->bdev);

	/* deinitialize trace producers */
	deinit_tracers(state);

	mutex_unlock(&state->client_mutex);
}

/**
 * @brief Initialize tracing context structures
 *
 * @param iotrace main iotrace context
 *
 * @retval 0 Tracing context initialized successfully
 * @retval non-zero Error code
 */
int iotrace_trace_init(struct iotrace_context *iotrace)
{
	int result;

	struct iotrace_state *state = &iotrace->trace_state;
	mutex_init(&state->client_mutex);

	/* Initialize buffer with version information */
	iotrace->version_buff_size = VERSION_BUFFER_MAX_LEN;
	iotrace->version_buff = vzalloc(iotrace->version_buff_size);
	if (!iotrace->version_buff) {
		iotrace->version_buff_size = 0;
		return -ENOMEM;
	}

	result = snprintf(iotrace->version_buff, iotrace->version_buff_size,
			  "%d\n%d\n%016llX\n", IOTRACE_EVENT_VERSION_MAJOR,
			  IOTRACE_EVENT_VERSION_MINOR, IOTRACE_MAGIC);

	return 0;
}

/**
 * @brief Deinitialize tracing context structures
 *
 * @param iotrace main iotrace context
 */
void iotrace_trace_deinit(struct iotrace_context *iotrace)
{
	if (iotrace->version_buff) {
		vfree(iotrace->version_buff);
		iotrace->version_buff = NULL;
		iotrace->version_buff_size = 0;
	}
}

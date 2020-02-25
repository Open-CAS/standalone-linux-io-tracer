/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "io_trace.h"
#include <linux/atomic.h>
#include <linux/tracepoint.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <trace/events/block.h>
#include "config.h"
#include "context.h"
#include "iotrace_event.h"
#include "procfs.h"
#include "procfs_files.h"
#include "trace.h"
#include "trace_bio.h"

static inline void iotrace_notify_of_new_events(struct iotrace_context *context,
                                                unsigned int cpu) {
    struct iotrace_cpu_context *cpu_context =
            per_cpu_ptr(context->cpu_context, cpu);
    octf_trace_t handle = *per_cpu_ptr(context->trace_state.traces, cpu);

    if (1 != octf_trace_is_almost_full(handle)) {
        return;
    }

    /* If process is waiting for traces, reset the flag, notify the process */
    if (atomic_cmpxchg(&cpu_context->waiting_for_trace, 1, 0)) {
        wake_up(&cpu_context->wait_queue);
    }
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
int iotrace_trace_desc(struct iotrace_context *iotrace,
                       unsigned cpu,
                       uint64_t dev_id,
                       const char *dev_name,
                       uint64_t dev_size) {
    int result = 0;
    struct iotrace_state *state = &iotrace->trace_state;
    octf_trace_t trace = *per_cpu_ptr(state->traces, cpu);
    octf_trace_event_handle_t ev_hndl;

    struct iotrace_event_device_desc *desc = NULL;
    uint64_t sid = atomic64_inc_return(&state->sid);

    const size_t dev_name_size = sizeof(desc->device_name);

    if (strnlen(dev_name, dev_name_size) >= dev_name_size)
        return -ENOSPC;

    result = octf_trace_get_wr_buffer(trace, &ev_hndl, (void **) &desc,
                                      sizeof(*desc));
    if (result) {
        return result;
    }
    strlcpy(desc->device_name, dev_name, sizeof(desc->device_name));

    iotrace_event_init_hdr(&desc->hdr, iotrace_event_type_device_desc, sid,
                           ktime_to_ns(ktime_get()), sizeof(*desc));

    desc->id = dev_id;
    desc->device_size = dev_size;

    result = octf_trace_commit_wr_buffer(trace, ev_hndl);

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
static void bio_queue_event(void *ignore,
                            struct request_queue *q,
                            struct bio *bio) {
    uint64_t dev_id;
    unsigned cpu = get_cpu();
    struct iotrace_context *iotrace = iotrace_get_context();

    if (iotrace_bdev_is_added(&iotrace->bdev, cpu, q)) {
        dev_id = disk_devt(IOTRACE_BIO_GET_DEV(bio));

        iotrace_trace_bio(iotrace, cpu, dev_id, bio);
        iotrace_notify_of_new_events(iotrace, cpu);
    }

    put_cpu();

    return;
}

/**
 * @brief Function registered to be called each time BIO is completed
 *
 * @param ignore Not used
 * @param q Associated request queue
 * @param bio Completed BIO
 * @param error result of BIO
 */
static void bio_complete_event(void *ignore,
                               struct request_queue *q,
                               struct bio *bio,
                               int error) {
    struct gendisk *disk;
    uint64_t dev_id;
    unsigned cpu = get_cpu();
    struct iotrace_context *iotrace = iotrace_get_context();

    if (iotrace_bdev_is_added(&iotrace->bdev, cpu, q)) {
        disk = IOTRACE_BIO_GET_DEV(bio);
        if (disk) {
            dev_id = disk_devt(disk);
        } else {
            dev_id = (uint64_t) q->dev;
        }

        iotrace_trace_bio_completion(iotrace, cpu, dev_id, bio, error);
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
static void deinit_tracers(struct iotrace_state *state) {
    unsigned i;

    for_each_online_cpu(i) {
        octf_trace_close(per_cpu_ptr(state->traces, i));
        iotrace_destroy_inode_tracer(per_cpu_ptr(state->inode_traces, i));
    }

    free_percpu(state->traces);
}

/**
 * @brief Initialize iotrace tracers
 *
 * Open iotrace objects (one per each procfs file)
 *
 * @param context iotrace context
 *
 * @retval 0 Tracers initialized successfully
 * @retval non-zero Error code
 */
static int init_tracers(struct iotrace_context *context) {
    int result = -EINVAL;
    unsigned i;
    octf_trace_t *trace;
    iotrace_inode_tracer_t *iotrace_inode;
    struct iotrace_proc_file *file;
    struct iotrace_state *state = &context->trace_state;

    state->traces = alloc_percpu(octf_trace_t);
    state->inode_traces = alloc_percpu(iotrace_inode_tracer_t);
    if (!state->traces || !state->inode_traces) {
        result = -ENOMEM;
        goto ERROR;
    }

    for_each_online_cpu(i) {
        struct iotrace_cpu_context *cpu_context =
                per_cpu_ptr(context->cpu_context, i);

        trace = per_cpu_ptr(state->traces, i);
        file = &cpu_context->proc_files;

        if (!file->trace_ring) {
            printk(KERN_ERR "Trace buffer is not allocated\n");
            result = -EINVAL;
            break;
        }

        result = octf_trace_open(file->trace_ring, file->trace_ring_size,
                                 file->consumer_hdr,
                                 octf_trace_open_mode_producer, trace);
        if (result)
            break;

        iotrace_inode = per_cpu_ptr(state->inode_traces, i);
        result = iotrace_create_inode_tracer(iotrace_inode, i);
        if (result) {
            break;
        }
    }

    if (result) {
    ERROR:
        if (state->traces) {
            for_each_online_cpu(i) {
                trace = per_cpu_ptr(state->traces, i);
                octf_trace_close(trace);
            }
            free_percpu(state->traces);
        }

        if (state->inode_traces) {
            for_each_online_cpu(i) {
                iotrace_inode = per_cpu_ptr(state->inode_traces, i);
                iotrace_destroy_inode_tracer(iotrace_inode);
            }

            free_percpu(state->inode_traces);
        }

        return result;
    }

    return 0;
}

static int iotrace_set_buffer_size(struct iotrace_context *iotrace,
                                   uint64_t size_mb) {
    unsigned long long size = size_mb * 1024ULL * 1024ULL /
                              (unsigned long long) num_online_cpus();

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
uint64_t iotrace_get_buffer_size(struct iotrace_context *iotrace) {
    return iotrace_get_context()->size * num_online_cpus() / 1024ULL / 1024ULL;
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
int iotrace_init_buffers(struct iotrace_context *iotrace, uint64_t size) {
    struct iotrace_state *state = &iotrace->trace_state;
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

    for_each_online_cpu(i) {
        struct iotrace_cpu_context *cpu_context =
                per_cpu_ptr(iotrace->cpu_context, i);

        result = iotrace_procfs_trace_file_alloc(&cpu_context->proc_files,
                                                 iotrace->size, i);
        if (result)
            break;
    }

exit:
    mutex_unlock(&state->client_mutex);
    return result;
}

static int _register_trace_points(void) {
    int result;

    result = iotrace_register_trace_block_bio_queue(bio_queue_event);
    if (result) {
        goto REG_BIO_QUEUE_ERROR;
    }

    result = iotrace_register_trace_block_bio_complete(bio_complete_event);
    if (result) {
        goto REG_BIO_COMPLETE_ERROR;
    }

    return 0;

REG_BIO_COMPLETE_ERROR:
    iotrace_unregister_trace_block_bio_queue(bio_queue_event);
REG_BIO_QUEUE_ERROR:
    return result;
}

static void _unregister_trace_points(void) {
    iotrace_unregister_trace_block_bio_queue(bio_queue_event);
    iotrace_unregister_trace_block_bio_complete(bio_complete_event);
}

/**
 * @brief Initialize tracing for new consumer
 *
 * @param iotrace main iotrace context
 *
 */
int iotrace_attach_client(struct iotrace_context *iotrace) {
    struct iotrace_state *state = &iotrace->trace_state;
    int result = 0;

    mutex_lock(&state->client_mutex);

    if (!state->clients) {
        result = init_tracers(iotrace);
        if (result)
            goto exit;

        result = _register_trace_points();
        if (result) {
            printk(KERN_ERR "Failed to register trace probe: %d\n", result);
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
void iotrace_detach_client(struct iotrace_context *iotrace) {
    struct iotrace_state *state = &iotrace->trace_state;
    mutex_lock(&state->client_mutex);

    state->clients--;
    if (state->clients) {
        mutex_unlock(&state->client_mutex);
        return;
    }

    /* unregister callback */
    _unregister_trace_points();
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
int iotrace_trace_init(struct iotrace_context *iotrace) {
    struct iotrace_state *state = &iotrace->trace_state;

    mutex_init(&state->client_mutex);

    return 0;
}

/**
 * @brief Deinitialize tracing context structures
 *
 * @param iotrace main iotrace context
 */
void iotrace_trace_deinit(struct iotrace_context *iotrace) {}

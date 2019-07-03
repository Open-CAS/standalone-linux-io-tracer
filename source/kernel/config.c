/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "config.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#define IOTRACE_BIO_TRACE_COMPLETION(bio) true
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
#define IOTRACE_BIO_TRACE_COMPLETION(bio) bio_flagged(bio, BIO_TRACE_COMPLETION)
#endif

/*
 * Request completion trace function
 */
static void _iotrace_block_rq_complete(void *data, struct request *rq,
        int error, unsigned int nr_bytes) {
    iotrace_bio_complete_fn trace_bio = data;
    struct bio *bio = rq->bio;

    while (bio) {
        unsigned bio_bytes = IOTRACE_BIO_BISIZE(bio);
        unsigned bytes = min(bio_bytes, nr_bytes);

        if (IOTRACE_BIO_TRACE_COMPLETION(bio)) {
            trace_bio(NULL, rq->q, bio, error);
        }

        bio = bio->bi_next;

        nr_bytes -= bytes;

        if (!nr_bytes) {
            break;
        }
    }
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
void iotrace_block_rq_complete(void *data, struct request_queue *q,
        struct request *rq, unsigned int nr_bytes) {
    _iotrace_block_rq_complete(data, rq, rq->errors, nr_bytes);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void iotrace_block_rq_complete(void *data, struct request *rq, int error,
        unsigned int nr_bytes) {
    _iotrace_block_rq_complete(data, rq, error, nr_bytes);
}
#endif

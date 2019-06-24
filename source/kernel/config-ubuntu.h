/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_KERNEL_INTERNAL_BIO_H
#define SOURCE_KERNEL_INTERNAL_BIO_H

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/kallsyms.h>
#include <linux/version.h>
#include <trace/events/block.h>

#ifndef SECTOR_SHIFT
#define SECTOR_SHIFT 9ULL
#endif
#ifndef SECTOR_SIZE
#define SECTOR_SIZE (1ULL << SECTOR_SHIFT)
#endif

#define IOTRACE_BIO_OP_FLAGS(bio) (bio)->bi_opf

/* BIO operation macros (read/write/discard) */
#define IOTRACE_BIO_IS_WRITE(bio) (bio_data_dir(bio) == WRITE)
#define IOTRACE_BIO_IS_DISCARD(bio) (bio_op(bio) == REQ_OP_DISCARD)

/* BIO attributes macros (address, size ...) */
#define IOTRACE_BIO_BISIZE(bio) (bio)->bi_iter.bi_size
#define IOTRACE_BIO_BISECTOR(bio) (bio)->bi_iter.bi_sector

/* BIO flags macros (flush, fua, ...) */
#define IOTRACE_BIO_IS_FLUSH(bio) ((IOTRACE_BIO_OP_FLAGS(bio)) & REQ_OP_FLUSH)
#define IOTRACE_BIO_IS_FUA(bio) ((IOTRACE_BIO_OP_FLAGS(bio)) & REQ_FUA)

/* Gets BIO vector  */
#define IOTRACE_BIO_BVEC(vec) (vec)
#define IOTRACE_BIO_GET_DEV(bio) bio->bi_disk

#define IOTRACE_LOOKUP_BDEV(path) lookup_bdev(path, 0)

static inline int iotrace_register_trace_block_bio_queue(
        void (*fn)(void *ignore, struct request_queue *, struct bio *)) {
    char *sym_name = "__tracepoint_block_bio_queue";
    typeof(&__tracepoint_block_bio_queue) tracepoint =
            (void *) kallsyms_lookup_name(sym_name);
    return tracepoint_probe_register((void *) tracepoint, fn, NULL);
}

static inline int iotrace_unregister_trace_block_bio_queue(
        void (*fn)(void *ignore, struct request_queue *, struct bio *)) {
    char *sym_name = "__tracepoint_block_bio_queue";
    typeof(&__tracepoint_block_bio_queue) tracepoint =
            (void *) kallsyms_lookup_name(sym_name);
    return tracepoint_probe_unregister((void *) tracepoint, fn, NULL);
}

#endif  // SOURCE_KERNEL_INTERNAL_BIO_H

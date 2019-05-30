/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include "internal/bio.h"
#include "internal/context.h"
#include "iotrace_event.h"

/**
 * @note IO classification defined by Differentiated Storage Services (DSS)
 */
enum {
    DSS_UNCLASSIFIED = 0,
    DSS_METADATA = 1,
    DSS_DATA_DIR = 7,
    DSS_DATA_FILE_4KB = 11,
    DSS_DATA_FILE_16KB = 12,
    DSS_DATA_FILE_64KB = 13,
    DSS_DATA_FILE_256KB = 14,
    DSS_DATA_FILE_1MB = 15,
    DSS_DATA_FILE_4MB = 16,
    DSS_DATA_FILE_16MB = 17,
    DSS_DATA_FILE_64MB = 18,
    DSS_DATA_FILE_256MB = 19,
    DSS_DATA_FILE_1GB = 20,
    DSS_DATA_FILE_BULK = 21,
    DSS_DATA_DIRECT = 22,
    DSS_MISC = 23,
    DSS_MAX = 33,
};

/* DSS: tagging according to file sizes */
static inline int _file_size_to_io_class(struct inode *inode)
{
	int tag = DSS_MISC;

	if (!inode) {
		printk(KERN_ERR "No INODE\n");
		return tag;
	}

	/* DSS.4 (note that inode 8 (the journal) passes through here) */
	if (S_ISREG(inode->i_mode)) {
		/* file size classification */
		loff_t size = i_size_read(inode);

		/* TODO get rid of assembler code */
#ifdef CONFIG_X86_64
		__asm__("movq %1, %%rax;"
			"subq $1, %%rax;"
			"orq $(4095), %%rax;"
			"bsrq %%rax, %%rax;"
			"shrl $1, %%eax;"
			"addl $6, %%eax;"
			"movl $21, %%ecx;"
			"cmpl %%ecx, %%eax;"
			"cmovgel %%ecx, %%eax;"
			"movl %%eax, %0"
			: "=r"(tag)
			: "r"(size)
			: "%rax", "%rcx");
#else
		if (size <= 1024 * 1048576) {
			unsigned int bits =
				__builtin_clzl((size - 1) | (4096 - 1));

			tag = 64 - bits + DSS_DATA_FILE_4KB;
			tag >>= 1;
		} else {
			tag = DSS_DATA_FILE_BULK;
		}
#endif
	} else if (S_ISDIR(inode->i_mode)) {
		tag = DSS_DATA_DIR;
	} else {
		tag = DSS_MISC;
	}
	return tag;
}

struct bio_info {
    struct inode *inode;
    struct page *page;
};

static uint32_t _get_dss_io_class(struct bio *bio, struct bio_info *info)
{
	/* Try get inode of block device */
	struct inode *bio_inode = NULL;
	struct page *page = NULL;

	if (!bio)
		return DSS_UNCLASSIFIED;

	/* Check if BIO carries data */
	if (!IOTRACE_BIO_BVEC(bio_iovec(bio)))
		return DSS_UNCLASSIFIED;

	page = bio_page(bio);

	if (!page)
		return DSS_UNCLASSIFIED;

	if (PageAnon(page))
		return DSS_DATA_DIRECT;

	if (PageSlab(page) || PageCompound(page)) {
		/* A filesystem issues IO on pages that does not belongs
		 * to the file page cache. It means that it is a
		 * part of metadata
		 */
		return DSS_METADATA;
	}

	if (!page->mapping) {
		/* XFS case, pages are allocated internally and do not
		 * have references into inode
		 */
		return DSS_METADATA;
	}

	bio_inode = page->mapping->host;
	if (!bio_inode)
		return DSS_UNCLASSIFIED;

	if (S_ISBLK(bio_inode->i_mode)) {
		/* EXT3 and EXT4 case. Metadata IO is performed into pages
		 * of block device cache
		 */
		return DSS_METADATA;
	}

	info->page = page;
	info->inode = bio_inode;

	return _file_size_to_io_class(bio_inode);
}

static void _trace_bio_fs_meta(octf_trace_t trace, log_sid_t sid, log_sid_t ref_sid,
        struct bio_info* info)
{
    struct iotrace_event_fs_meta ev = {};

    iotrace_event_init_hdr(&ev.hdr, iotrace_event_type_fs_meta,
            sid, ktime_to_ns(ktime_get()), sizeof(ev));

    ev.ref_sid = ref_sid;
    ev.file_id = info->inode->i_ino;
    ev.file_offset = info->page->index << (PAGE_SHIFT - SECTOR_SHIFT);
    ev.file_size = info->inode->i_size >> SECTOR_SHIFT;

    octf_trace_push(trace, &ev, sizeof(ev));
}

void iotrace_trace_bio(struct iotrace_context *context, unsigned cpu,
                  uint64_t dev_id, struct bio *bio)
{
    struct iotrace_event ev = {};
    struct iotrace_state *state = &context->trace_state;
    uint64_t sid = atomic64_inc_return(&state->sid);
    struct bio_info info;
    octf_trace_t trace = *per_cpu_ptr(state->traces, cpu);

    iotrace_event_init_hdr(&ev.hdr, iotrace_event_type_io, sid,
              ktime_to_ns(ktime_get()), sizeof(ev));

    if (IOTRACE_BIO_IS_DISCARD(bio))
        ev.operation = iotrace_event_operation_discard;
    else if (IOTRACE_BIO_IS_WRITE(bio))
        ev.operation = iotrace_event_operation_wr;
    else
        ev.operation = iotrace_event_operation_rd;

    if (IOTRACE_BIO_IS_FLUSH(bio))
        ev.flags |= iotrace_event_flag_flush;
    if (IOTRACE_BIO_IS_FUA(bio))
        ev.flags |= iotrace_event_flag_fua;

    ev.lba = IOTRACE_BIO_BISECTOR(bio);
    ev.len = IOTRACE_BIO_BISIZE(bio) >> SECTOR_SHIFT;
    ev.io_class = _get_dss_io_class(bio, &info);
    ev.dev_id = dev_id;

    octf_trace_push(trace, &ev, sizeof(ev));

    if (ev.io_class >= DSS_DATA_FILE_4KB && ev.io_class <= DSS_DATA_FILE_BULK)
        _trace_bio_fs_meta(trace, atomic64_inc_return(&state->sid), sid, &info);
}

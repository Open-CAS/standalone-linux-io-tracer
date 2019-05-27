/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/version.h>
#include <linux/blkdev.h>
#include "internal/dss.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#define SEGMENT_BVEC(vec) (&(vec))
#else
#define SEGMENT_BVEC(vec) (vec)
#endif

/* DSS: tagging according to file sizes */
static inline int iotrace_dss_file_size_tag(struct inode *inode)
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

int iotrace_dss_bio_io_class(struct bio *bio)
{
	/* Try get inode of block device */
	struct inode *bio_inode = NULL;
	struct page *page = NULL;

	if (!bio)
		return DSS_UNCLASSIFIED;

	/* Try get inode of BIO page */
	if (!SEGMENT_BVEC(bio_iovec(bio)))
		return DSS_UNCLASSIFIED;

	/* TODO consider all pages in BIO, not just the first one */
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

	return iotrace_dss_file_size_tag(bio_inode);
}

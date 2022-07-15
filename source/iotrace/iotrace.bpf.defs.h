/*
 * Copyright(c) 2012-2020 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_USERSPACE_IOTRACE_BPF_DEFS_H_
#define SOURCE_USERSPACE_IOTRACE_BPF_DEFS_H_

#include "vmlinux.h"

#include <bpf/bpf_core_read.h>

#ifndef MIN
#define MIN(x, y)                \
    ({                           \
        __typeof__(x) __x = (x); \
        __typeof__(y) __y = (y); \
        __x < __y ? __x : __y;   \
    })
#endif

#ifndef __always_inline
#define __always_inline inline
#endif

#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE (1ULL << PAGE_SHIFT)
#endif

#ifndef SECTOR_SHIFT
#define SECTOR_SHIFT 9
#endif

#ifndef SECTOR_SIZE
#define SECTOR_SIZE (1ULL << SECTOR_SHIFT)
#endif

#ifndef __always_inline
#define __always_inline inline
#endif

#define S_IFMT 00170000
#define S_IFREG 0100000
#define S_IFBLK 0060000
#define S_IFDIR 0040000

#define S_ISREG(m) (((m) &S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) &S_IFMT) == S_IFDIR)
#define S_ISBLK(m) (((m) &S_IFMT) == S_IFBLK)

/*
 * BIO definitions
 */
static __always_inline uint8_t iotrace_bio_op(struct bio *bio) {
#define REQ_OP_BITS 8
#define REQ_OP_MASK ((1 << REQ_OP_BITS) - 1)

    return BPF_CORE_READ(bio, bi_opf) & REQ_OP_MASK;
}

static __always_inline bool iotrace_bio_is_discard(struct bio *bio) {
    return iotrace_bio_op(bio) == REQ_OP_DISCARD;
}

static __always_inline bool iotrace_bio_is_write(struct bio *bio) {
    return iotrace_bio_op(bio) == REQ_OP_WRITE;
}

static __always_inline bool iotrace_bio_is_flush(struct bio *bio) {
#define REQ_PREFLUSH (1ULL << __REQ_PREFLUSH)
#define REQ_FUA (1ULL << __REQ_FUA)

    if (BPF_CORE_READ(bio, bi_opf) & (REQ_FUA | REQ_PREFLUSH)) {
        return true;
    } else {
        return false;
    }
}

static inline bool iotrace_bio_has_data(struct bio *bio) {
    uint8_t op = iotrace_bio_op(bio);

    if (bio && BPF_CORE_READ(bio, bi_iter.bi_size) && op != REQ_OP_DISCARD &&
        op != REQ_OP_SECURE_ERASE && op != REQ_OP_WRITE_ZEROES) {
        return true;
    }

    return false;
}

static __always_inline bool iotrace_bio_is_fua(struct bio *bio) {
    if (BPF_CORE_READ(bio, bi_opf) & REQ_FUA) {
        return true;
    } else {
        return false;
    }
}

static __always_inline bool iotrace_bio_flagged(struct bio *bio,
                                                uint32_t flag) {
    uint64_t flags = BPF_CORE_READ(bio, bi_flags);

    return (flags & (1U << flag)) != 0;
}

static __always_inline int iotrace_bio_error(struct bio *bio) {
    uint64_t status = BPF_CORE_READ(bio, bi_status);

    return status ? 1 : 0;
}

static __always_inline struct page *iotrace_bio_page(struct bio *bio) {
    uint32_t idx = BPF_CORE_READ(bio, bi_iter.bi_idx);
    uint64_t offset = BPF_CORE_READ(bio, bi_io_vec[idx].bv_offset);
    uint64_t done = BPF_CORE_READ(bio, bi_iter.bi_bvec_done);
    uint64_t page_off = (offset + done) / 4096ULL;

    struct page *page = BPF_CORE_READ(bio, bi_io_vec[idx].bv_page);

    return page + page_off;
}

/*
 * Request definitions
 */

static __always_inline uint8_t iotrace_rq_op(struct request *rq) {
    return BPF_CORE_READ(rq, cmd_flags) & REQ_OP_MASK;
}

static __always_inline bool iotrace_rq_is_discard(struct request *rq) {
    return iotrace_rq_op(rq) == REQ_OP_DISCARD;
}

static __always_inline bool iotrace_rq_is_write(struct request *rq) {
    return iotrace_rq_op(rq) == REQ_OP_WRITE;
}

static __always_inline bool iotrace_rq_is_flush(struct request *rq) {
    if (BPF_CORE_READ(rq, cmd_flags) & (REQ_FUA | REQ_PREFLUSH)) {
        return true;
    } else {
        return false;
    }
}

static __always_inline bool iotrace_rq_is_fua(struct request *rq) {
    if (BPF_CORE_READ(rq, cmd_flags) & REQ_FUA) {
        return true;
    } else {
        return false;
    }
}

static __always_inline dev_t iotrace_rq_to_dev_id(const struct request *rq) {
    struct gendisk *disk = BPF_CORE_READ(rq, q, disk);

    return MKDEV(BPF_CORE_READ(disk, major), BPF_CORE_READ(disk, first_minor));
}

static __always_inline uint64_t iotrace_rq_to_id(const struct request *rq) {
    return (uint64_t) rq;
}

static __always_inline uint8_t iotrace_rq_write_hint(struct request *rq) {
    return BPF_CORE_READ(rq, write_hint);
}

/*
 * Page definitions
 */

static __always_inline unsigned long _compound_head(const struct page *page) {
    unsigned long head = BPF_CORE_READ(page, compound_head);

    if (head & 1)
        return head - 1;
    return (unsigned long) page;
}

static __always_inline struct folio *page_folio(struct page *page) {
    return (struct folio *) _compound_head(page);
}

static __always_inline bool folio_test_anon(struct folio *folio) {
#define PAGE_MAPPING_ANON 0x1
    struct address_space *mapping = BPF_CORE_READ(folio, mapping);

    return ((unsigned long) mapping & PAGE_MAPPING_ANON) != 0;
}

static __always_inline bool iotrace_page_anon(struct page *page) {
    return folio_test_anon(page_folio(page));
}

static __always_inline int iotrace_page_read_ahead(struct page *page) {
    return BPF_CORE_READ(page, flags) & (1UL < PG_readahead);
}

static __always_inline int iotrace_page_tail(struct page *page) {
    return BPF_CORE_READ(page, compound_head) & 1;
}

static __always_inline int iotrace_page_compound(struct page *page) {
    if (BPF_CORE_READ(page, flags) & (1UL < PG_head)) {
        return true;
    }

    if (iotrace_page_tail(page)) {
        return true;
    }

    return false;
}

static __always_inline bool iotrace_page_slab(struct page *page) {
    if (BPF_CORE_READ(page, flags) & (1UL < PG_slab)) {
        return true;
    } else {
        return false;
    }
}

static __always_inline bool iotrace_page_has_mapping(struct page *page) {
    struct folio *folio = page_folio(page);

    return NULL != BPF_CORE_READ(folio, mapping);
}

static __always_inline struct inode *iotrace_page_inode(struct page *page) {
    struct folio *folio = page_folio(page);
    struct inode *inode = BPF_CORE_READ(page, mapping, host);

    return inode;
}

static __always_inline uint64_t iotrace_page_index(struct page *page) {
    return BPF_CORE_READ(page, index);
}

/*
 * Inode definitions
 */
static __always_inline struct block_device *iotrace_inode_bdev(
        struct inode *inode) {
    return BPF_CORE_READ(inode, i_sb, s_bdev);
}

static __always_inline uint64_t iotrace_inode_no(struct inode *inode) {
    return BPF_CORE_READ(inode, i_ino);
}

static __always_inline void iotrace_inode_ctime(struct inode *inode,
                                                struct timespec64 *cTime) {
    *cTime = BPF_CORE_READ(inode, i_ctime);
}

static __always_inline uint64_t iotrace_inode_dev(struct inode *inode) {
    return BPF_CORE_READ(inode, i_sb, s_dev);
}

static __always_inline uint64_t iotrace_inode_size(struct inode *inode) {
    return BPF_CORE_READ(inode, i_size);
}

/*
 * Block device definitions
 */
static __always_inline uint64_t iotrace_bdev_id(struct block_device *bdev) {
    return BPF_CORE_READ(bdev, bd_dev);
}

static __always_inline struct block_device *iotrace_bdev_whole(
        struct block_device *bdev) {
    return BPF_CORE_READ(bdev, bd_disk, part0);
}

static __always_inline bool iotrace_bdev_is_partition(
        struct block_device *bdev) {
    if (BPF_CORE_READ(bdev, bd_partno)) {
        return true;
    } else {
        return false;
    }
}

/*
 * dentry definitions
 */
static __always_inline struct block_device *iotrace_dentry_bdev(
        struct dentry *dentry) {
    return BPF_CORE_READ(dentry, d_sb, s_bdev);
}

static __always_inline bool iotrace_dentry_is_root(struct dentry *dentry) {
    return dentry == BPF_CORE_READ(dentry, d_parent);
}

#define hlist_entry(ptr, type, member) container_of(ptr, type, member);

static inline bool iotrace_dentry_alias_list_empty(struct inode *inode) {
    struct hlist_node *first = BPF_CORE_READ(inode, i_dentry.first);

    return NULL == first;
}

static struct dentry *iotrace_dentry_find_any_alias(struct inode *inode) {
    struct hlist_node *first = BPF_CORE_READ(inode, i_dentry.first);

    if (first) {
        return hlist_entry(first, struct dentry, d_u.d_alias);
    }

    return NULL;
}

#define hlist_for_each_entry(pos, head, member)                              \
    for (pos = hlist_entry_safe((head)->first, typeof(*(pos)), member); pos; \
         pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

static inline bool d_unhashed(const struct dentry *dentry) {
    return !BPF_CORE_READ(dentry, d_hash.pprev);
}

static __always_inline struct dentry *iotrace_dentry_get_alias(
        struct inode *inode) {
    if (iotrace_dentry_alias_list_empty(inode)) {
        return NULL;
    }

    if (S_ISDIR(BPF_CORE_READ(inode, i_mode))) {
        return iotrace_dentry_find_any_alias(inode);
    }

    struct hlist_node *iter = BPF_CORE_READ(inode, i_dentry.first);
    struct dentry *found = NULL;
    struct dentry *alias = hlist_entry(iter, struct dentry, d_u.d_alias);

    for (int i = 0; i < 16; i++) {
        if (!d_unhashed(alias)) {
            found = alias;
            break;
        }

        iter = BPF_CORE_READ(alias, d_u.d_alias.next);
        struct dentry *alias = hlist_entry(iter, struct dentry, d_u.d_alias);
    }

    return found;
}

#endif /* SOURCE_USERSPACE_IOTRACE_BPF_DEFS_H_ */

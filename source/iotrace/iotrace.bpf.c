/*
 * Copyright(c) 2012-2020 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "vmlinux.h"

#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "iotrace.bpf.common.h"
#include "iotrace.bpf.config.h"
#include "iotrace.bpf.defs.h"
#include "iotrace_event.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct iotrace_inode_info {
    bool traced;
};

struct {
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_INODE_STORAGE);
    __uint(map_flags, BPF_F_NO_PREALLOC);
    __type(key, int);
    __type(value, struct iotrace_inode_info);
} inode_storage_map SEC(".maps");

uint64_t ref_sid = 0;
uint64_t device[32] = {0};
uint64_t timebase; /* TODO(mbarczak) Make this per-cpu variable */

static __always_inline void iotrace_inode_set_traced(struct inode *inode) {
    if (inode) {
        struct iotrace_inode_info *info =
                bpf_inode_storage_get(&inode_storage_map, inode, NULL,
                                      BPF_LOCAL_STORAGE_GET_F_CREATE);

        if (info) {
            info->traced = true;
        }
    }
}

static __always_inline bool iotrace_inode_is_traced(struct inode *inode) {
    struct iotrace_inode_info *info = NULL;
    int value = 0;

    info = bpf_inode_storage_get(&inode_storage_map, inode, 0, 0);
    if (info) {
        return info->traced;
    }

    return false;
}

static __always_inline uint64_t iotrace_ktime_get_ns(void) {
    if (0 == timebase) {
        timebase = bpf_ktime_get_ns();
    }

    return bpf_ktime_get_ns() - timebase;
}

static __always_inline bool iotrace_dev_to_trace(const dev_t dev) {
    uint64_t i;

    for (i = 0; i < sizeof(device) / sizeof(device[0]); i++) {
        if (dev == device[i]) {
            return true;
        } else if (0 == device[i]) {
            break;
        }
    }
    return false;
}

static __always_inline uint64_t iotrace_event_get_seq_id(void) {
    return __sync_add_and_fetch(&ref_sid, 1);
}

static __always_inline dev_t iotrace_bio_to_dev_id(const struct bio *bio) {
    struct block_device *bdev = BPF_CORE_READ(bio, bi_bdev);
    struct gendisk *disk = BPF_CORE_READ(bdev, bd_disk);

    return MKDEV(BPF_CORE_READ(disk, major), BPF_CORE_READ(disk, first_minor));
}

static __always_inline uint64_t iotrace_bio_to_id(const struct bio *bio) {
    return (uint64_t) bio;
}

struct iotrace_bio_fs_link {
    struct inode *inode;
    struct page *page;
    bool metadata;
    bool direct;
    bool readahead;
};

static __always_inline void iotrace_bio_get_fs_link(
        struct bio *bio,
        struct iotrace_bio_fs_link *link) {
    struct page *page = iotrace_bio_page(bio);

    if (!page) {
        return;
    }
    link->page = page;

    if (iotrace_page_read_ahead(page)) {
        link->readahead = true;
    }

    if (iotrace_page_anon(page)) {
        link->direct = true;
        return;
    }

    if (iotrace_page_compound(page)) {
        link->metadata = true;
    }
    if (iotrace_page_slab(page)) {
        link->metadata = true;
    }
    if (!iotrace_page_has_mapping(page)) {
        /*
         * No mapping, page allocated by a filesystem and it is metadata
         */

        link->metadata = true;
        return;
    }

    struct inode *inode = iotrace_page_inode(page);
    if (!inode) {
        return;
    }

    if (S_ISBLK(BPF_CORE_READ(inode, i_mode))) {
        link->metadata = true;

    } else if (S_ISREG(BPF_CORE_READ(inode, i_mode))) {
        link->inode = inode;
        link->metadata = false;
    } else if (S_ISDIR(BPF_CORE_READ(inode, i_mode))) {
        link->metadata = true;
    }

    // TODO(mbarczak) Try to use flag REQ_META to map this to metadata IO
}

static __always_inline void iotrace_bio_trace_inode(void *ctx,
                                                    struct inode *inode,
                                                    struct page *page,
                                                    uint64_t ref_id) {
    struct iotrace_event_fs_meta ev;
    iotrace_event_init_hdr(&ev.hdr, iotrace_event_type_fs_meta,
                           iotrace_event_get_seq_id(), iotrace_ktime_get_ns(),
                           sizeof(ev));

    ev.ref_id = ref_id;
    ev.file_id.id = iotrace_inode_no(inode);

    struct timespec64 cTime;
    iotrace_inode_ctime(inode, &cTime);
    ev.file_id.ctime.tv_nsec = cTime.tv_nsec;
    ev.file_id.ctime.tv_sec = cTime.tv_sec;

    ev.file_offset = iotrace_page_index(page) << (PAGE_SHIFT - SECTOR_SHIFT);
    ev.file_size = iotrace_inode_size(inode) >> SECTOR_SHIFT;
    ev.partition_id = iotrace_inode_dev(inode);

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &ev, sizeof(ev));
}

static __always_inline void iotrace_bio_set_event(struct iotrace_event *ev,
                                                  struct bio *bio,
                                                  dev_t dev) {
    ev->id = iotrace_bio_to_id(bio);

    if (iotrace_bio_is_discard(bio)) {
        ev->operation = iotrace_event_operation_discard;
    } else if (iotrace_bio_is_write(bio)) {
        ev->operation = iotrace_event_operation_wr;
    } else {
        ev->operation = iotrace_event_operation_rd;
    }

    if (iotrace_bio_is_flush(bio)) {
        ev->flags |= iotrace_event_flag_flush;
    }
    if (iotrace_bio_is_fua(bio)) {
        ev->flags |= iotrace_event_flag_fua;
    }

    ev->lba = BPF_CORE_READ(bio, bi_iter.bi_sector);
    ev->len = BPF_CORE_READ(bio, bi_iter.bi_size) >> 9;
    ev->dev_id = dev;
    ev->write_hint = iotrace_bio_write_hint(bio);
}

SEC("tp_btf/block_bio_queue")
int BPF_PROG(block_bio_queue, struct bio *bio) {
    struct iotrace_event event = {0};
    struct iotrace_bio_fs_link link = {0};

    dev_t dev = iotrace_bio_to_dev_id(bio);

    if (!iotrace_dev_to_trace(dev)) {
        return 0;
    }

    if (iotrace_bio_has_data(bio)) {
        iotrace_bio_get_fs_link(bio, &link);
    }

    iotrace_event_init_hdr(&event.hdr, iotrace_event_type_io,
                           iotrace_event_get_seq_id(), iotrace_ktime_get_ns(),
                           sizeof(event));
    if (link.direct) {
        event.flags |= iotrace_event_flag_direct;
    }
    if (link.metadata) {
        event.flags |= iotrace_event_flag_metadata;
    }
    if (link.readahead) {
        event.flags |= iotrace_event_flag_readahead;
    }
    iotrace_bio_set_event(&event, bio, dev);

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &event,
                          sizeof(event));

    if (link.inode) {
        iotrace_bio_trace_inode(ctx, link.inode, link.page, event.id);
    }

    return 0;
}

static __always_inline void iotrace_bio_complete(void *ctx, struct bio *bio) {
    struct iotrace_event_completion cmpl = {0};
    dev_t dev = iotrace_bio_to_dev_id(bio);

    if (!iotrace_dev_to_trace(dev)) {
        return;
    }

    iotrace_event_init_hdr(&cmpl.hdr, iotrace_event_type_io_cmpl,
                           iotrace_event_get_seq_id(), iotrace_ktime_get_ns(),
                           sizeof(cmpl));

    cmpl.ref_id = iotrace_bio_to_id(bio);
    cmpl.lba = BPF_CORE_READ(bio, bi_iter.bi_sector);
    cmpl.len = BPF_CORE_READ(bio, bi_iter.bi_size) >> 9;
    cmpl.error = iotrace_bio_error(bio);
    cmpl.dev_id = dev;

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &cmpl, sizeof(cmpl));
}

SEC("tp_btf/block_bio_complete")
int BPF_PROG(block_bio_complete, struct request_queue *q, struct bio *bio) {
    iotrace_bio_complete(ctx, bio);
    return 0;
}

static __always_inline int iotrace_rq_set_event(struct iotrace_event *ev,
                                                struct request *rq,
                                                dev_t dev) {
    ev->id = iotrace_rq_to_id(rq);

    /* Only discard and flush expecteded here */
    if (iotrace_rq_is_discard(rq)) {
        ev->operation = iotrace_event_operation_discard;
        ev->lba = BPF_CORE_READ(rq, __sector);
        ev->len = BPF_CORE_READ(rq, __data_len) >> 9;
    } else if (iotrace_rq_is_flush(rq)) {
        ev->operation = iotrace_event_operation_wr;
        ev->flags |= iotrace_event_flag_flush;
    } else {
        return -1;
    }

    ev->dev_id = dev;
    ev->write_hint = iotrace_rq_write_hint(rq);

    return 0;
}

void static __always_inline iotrace_rq_queue(void *ctx, struct request *rq) {
    struct bio *bio = BPF_CORE_READ(rq, bio);
    if (bio) {
        /* This request contains BIO and is traced in block_bio_queue */
        return;
    }

    struct iotrace_event event = {0};
    dev_t dev = iotrace_rq_to_dev_id(rq);

    if (!iotrace_dev_to_trace(dev)) {
        return;
    }

    iotrace_event_init_hdr(&event.hdr, iotrace_event_type_io,
                           iotrace_event_get_seq_id(), iotrace_ktime_get_ns(),
                           sizeof(event));

    if (iotrace_rq_set_event(&event, rq, dev)) {
        return;
    }

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &event,
                          sizeof(event));
}

SEC("tp_btf/block_rq_issue")
int BPF_PROG(block_rq_issue, struct request *rq) {
    iotrace_rq_queue(ctx, rq);
    return 0;
}

void static __always_inline iotrace_rq_complete(void *ctx,
                                                struct request *rq,
                                                int error) {
    struct iotrace_event_completion cmpl = {0};
    dev_t dev = iotrace_rq_to_dev_id(rq);

    if (!iotrace_dev_to_trace(dev)) {
        return;
    }

    iotrace_event_init_hdr(&cmpl.hdr, iotrace_event_type_io_cmpl,
                           iotrace_event_get_seq_id(), iotrace_ktime_get_ns(),
                           sizeof(cmpl));

    cmpl.ref_id = iotrace_rq_to_id(rq);

    if (iotrace_rq_is_discard(rq)) {
        cmpl.lba = BPF_CORE_READ(rq, __sector);
        cmpl.len = BPF_CORE_READ(rq, __data_len) >> 9;
    }
    cmpl.error = error;
    cmpl.dev_id = dev;

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &cmpl, sizeof(cmpl));
}

SEC("tp_btf/block_rq_complete")
int BPF_PROG(block_rq_complete,
             struct request *rq,
             int error,
             unsigned int nr_bytes) {
    struct bio *bio = BPF_CORE_READ(rq, bio);
    uint32_t i;

    if (NULL == bio) {
        iotrace_rq_complete(ctx, rq, error);
        return 0;
    }

    for (i = 0; i < 64; i++) {
        if (bio) {
            iotrace_bio_complete(ctx, bio);
            bio = BPF_CORE_READ(bio, bi_next);
        } else {
            break;
        }
    }

    return 0;
}

static __always_inline void iotrace_inode_set_event(
        struct iotrace_event_fs_file_name *ev,
        struct dentry *dentry,
        struct inode *inode,
        uint64_t part_id) {
    struct timespec64 cTime;

    ev->partition_id = part_id;
    ev->file_id.id = iotrace_inode_no(inode);
    iotrace_inode_ctime(inode, &cTime);
    ev->file_id.ctime.tv_nsec = cTime.tv_nsec;
    ev->file_id.ctime.tv_sec = cTime.tv_sec;

    /* Get parent */
    struct inode *parent = BPF_CORE_READ(dentry, d_parent, d_inode);

    ev->file_parent_id.id = iotrace_inode_no(parent);
    iotrace_inode_ctime(parent, &cTime);
    ev->file_parent_id.ctime.tv_nsec = cTime.tv_nsec;
    ev->file_parent_id.ctime.tv_sec = cTime.tv_sec;

    // Copy file name
    {
        size_t smax = BPF_CORE_READ(dentry, d_name.len);
        size_t dmax = sizeof(ev->file_name) - 1;
        size_t to_copy = smax;
        if (dmax < smax) {
            to_copy = dmax;
        }
        to_copy++;

        bpf_probe_read_str(ev->file_name, to_copy,
                           BPF_CORE_READ(dentry, d_name.name));
    }
}

static __always_inline int iotrace_inode(void *ctx,
                                         struct dentry *dentry,
                                         struct inode *inode) {
    struct block_device *bdev = iotrace_dentry_bdev(dentry);
    if (!bdev) {
        /* No bdev for this inode */
        return 1;
    }

    struct block_device *whole = iotrace_bdev_whole(bdev);
    dev_t part_id = iotrace_bdev_id(bdev);
    dev_t whole_id = iotrace_bdev_id(whole);

    if (!iotrace_dev_to_trace(whole_id)) {
        return 1;
    }

    struct iotrace_event_fs_file_name ev = {{0}};

    iotrace_event_init_hdr(&ev.hdr, iotrace_event_type_fs_file_name,
                           iotrace_event_get_seq_id(), iotrace_ktime_get_ns(),
                           sizeof(ev));
    iotrace_inode_set_event(&ev, dentry, inode, part_id);

    int result = bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &ev,
                                       sizeof(ev));

    return 0;
}

static __always_inline void iotrace_inode_loop(void *ctx,
                                               struct dentry *dentry,
                                               struct inode *inode) {
    for (uint32_t i = 0; i < 32; i++) {
        if (!dentry || !inode) {
            break;
        }

        if (iotrace_inode_is_traced(inode)) {
            return;
        }

        iotrace_inode_set_traced(inode);

        int result = iotrace_inode(ctx, dentry, inode);
        if (result) {
            break;
        }

        if (iotrace_dentry_is_root(dentry)) {
            break;
        }

        /* Try get parent */
        dentry = dentry->d_parent;
        if (dentry) {
            inode = dentry->d_inode;
        }
    }
}

SEC("lsm/file_open")
int BPF_PROG(file_open, struct file *file) {
    struct block_device *bdev =
            iotrace_inode_bdev(BPF_CORE_READ(file, f_inode));
    if (!bdev) {
        /* No bdev for this inode */
        return 0;
    }

    struct block_device *whole = iotrace_bdev_whole(bdev);
    dev_t part_id = iotrace_bdev_id(bdev);
    dev_t whole_id = iotrace_bdev_id(whole);

    if (!iotrace_dev_to_trace(whole_id)) {
        return 0;
    }

    iotrace_inode_loop(ctx, file->f_path.dentry, file->f_inode);
    return 0;
}

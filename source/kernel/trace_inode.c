#include "trace_inode.h"

#include <linux/atomic.h>
#include <linux/dcache.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/list.h>
#include <linux/slab.h>
#include "io_trace.h"
#include "iotrace_event.h"
#include "trace_env_kernel.h"

#ifndef DEBUG
#define DEBUG 0
#endif

#if 1 == DEBUG
#define debug(format, ...)                                               \
    printk(KERN_INFO "[iotrace][inode cache] %s " format "\n", __func__, \
           ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

#define CACHE_ENTRIES 4096ULL
#define HASH_ENTRIES CACHE_ENTRIES / 4ULL

struct cache_entry {
    struct list_head lru;
    struct hlist_node hash;
    uint64_t inode_id;
    dev_t device_id;
};

struct iotrace_inode {
    int cpu;
    DECLARE_HASHTABLE(hash_table, ilog2(HASH_ENTRIES));
    struct list_head list_free;
    struct list_head list_lru;
    struct cache_entry entries[CACHE_ENTRIES];
};

int iotrace_inode_create(iotrace_inode_t *_iotrace_inode, int cpu) {
    int i;
    struct iotrace_inode *iotrace_inode;

    debug();

    *_iotrace_inode = NULL;
    iotrace_inode = vzalloc_node(sizeof(*iotrace_inode), cpu_to_node(cpu));
    if (!iotrace_inode) {
        return -ENOMEM;
    }

    iotrace_inode->cpu = cpu;
    hash_init(iotrace_inode->hash_table);
    INIT_LIST_HEAD(&iotrace_inode->list_free);
    INIT_LIST_HEAD(&iotrace_inode->list_lru);

    /* Initialize free list */
    for (i = 0; i < ARRAY_SIZE(iotrace_inode->entries); i++) {
        struct cache_entry *entry = &iotrace_inode->entries[i];
        list_add(&entry->lru, &iotrace_inode->list_free);
    }

    *_iotrace_inode = iotrace_inode;
    return 0;
}

void iotrace_inode_destroy(iotrace_inode_t *iotrace_inode) {
    debug();

    if (*iotrace_inode) {
        vfree(*iotrace_inode);
        *iotrace_inode = NULL;
    }
}

static uint64_t _hash_index(struct inode *inode) {
    return inode->i_ino % HASH_ENTRIES;
}

static void _set_hot(iotrace_inode_t iotrace_inode, struct cache_entry *entry) {
    list_del(&entry->lru);
    list_add(&entry->lru, &iotrace_inode->list_lru);
}

static struct cache_entry *_get_entry(iotrace_inode_t iotrace_inode) {
    struct cache_entry *entry;
    struct list_head *free_list = &iotrace_inode->list_free;

    if (!list_empty(free_list)) {
        debug("Pick free");
        entry = list_first_entry(free_list, struct cache_entry, lru);
        list_del(&entry->lru);
        return entry;
    }

    // No more free entries, we need to evict one
    entry = list_last_entry(&iotrace_inode->list_lru, struct cache_entry, lru);

    debug("Remove %llu", entry->inode_id);

    list_del(&entry->lru);
    hash_del(&entry->hash);

    return entry;
}

static void _map(iotrace_inode_t iotrace_inode, struct inode *inode) {
    struct cache_entry *entry = _get_entry(iotrace_inode);
    uint64_t index = _hash_index(inode);

    entry->inode_id = inode->i_ino;
    entry->device_id = inode->i_sb->s_dev;

    list_add(&entry->lru, &iotrace_inode->list_lru);
    hash_add(iotrace_inode->hash_table, &entry->hash, index);

    debug("Map %lu", inode->i_ino);
}

static struct cache_entry *_lookup(iotrace_inode_t iotrace_inode,
                                   struct inode *inode) {
    uint64_t index = _hash_index(inode);
    struct cache_entry *entry = NULL;

    hash_for_each_possible(iotrace_inode->hash_table, entry, hash, index) {
        if (inode->i_ino == entry->inode_id &&
            inode->i_sb->s_dev == entry->device_id) {
            debug("Hit %lu", inode->i_ino);
            _set_hot(iotrace_inode, entry);
            return entry;
        }
    }

    debug("Miss %lu", inode->i_ino);
    return NULL;
}

int _trace(struct iotrace_state *state,
           octf_trace_t trace,
           uint64_t devId,
           uint64_t id,
           uint64_t parent_id,
           struct dentry *dentry) {
    struct iotrace_event_fs_file_name ev = {};
    uint64_t sid = atomic64_inc_return(&state->sid);

    iotrace_event_init_hdr(&ev.hdr, iotrace_event_type_fs_file_name, sid,
                           ktime_to_ns(ktime_get()), sizeof(ev));

    ev.device_id = devId;
    ev.file_id = id;
    ev.file_parent_id = parent_id;

    // Copy file name
    // TODO (mariuszbarczak) Instead of using short name we should use long one
    {
        size_t smax = sizeof(dentry->d_iname);
        size_t dmax = sizeof(ev.file_name) - 1;
        size_t to_copy = min(smax, dmax);
        memcpy_s(ev.file_name, dmax, dentry->d_iname, to_copy);
        ev.file_name[dmax] = '\0';
    }

    // TODO (mariuszbarczak) Switch to operate directly on OCTF trace buffer
    return octf_trace_push(trace, &ev, sizeof(ev));
}

void iotrace_inode_trace(struct iotrace_state *state,
                         octf_trace_t trace,
                         iotrace_inode_t iotrace_inode,
                         struct inode *this_inode) {
    int result;
    struct cache_entry *entry;
    struct dentry *this_dentry = NULL, *parent_dentry = NULL;
    struct inode *parent_inode = NULL;

    // Get dentry from inode
    this_dentry = d_find_alias(this_inode);
    if (!this_dentry) {
        // some error occurred, don't trace and return
        return;
    }

    do {
        entry = _lookup(iotrace_inode, this_inode);
        if (entry) {
            // inode already cached
            break;
        }

        // Get parent
        parent_dentry = dget_parent(this_dentry);
        if (parent_dentry) {
            parent_inode = d_inode(parent_dentry);
        }

        // Trace dentry name (file or directory name)
        debug("ID = %lu, name = %s", this_inode->i_ino, this_dentry->d_iname);

        result =
                _trace(state, trace, this_inode->i_sb->s_dev, this_inode->i_ino,
                       parent_inode ? parent_inode->i_ino : 0, this_dentry);

        if (0 == result) {
            // event traced successfully, add inode to the cache
            _map(iotrace_inode, this_inode);
        }

        // Switch to the parent inode
        this_inode = parent_inode;
        parent_inode = NULL;

        // Switch to the parent dentry
        dput(this_dentry);
        this_dentry = parent_dentry;
        parent_dentry = NULL;

    } while (this_inode && this_dentry);

    if (this_dentry) {
        dput(this_dentry);
    }

    if (parent_dentry) {
        dput(parent_dentry);
    }
}

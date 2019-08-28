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

/*
 * The cache logic based on hash tables with collision list. The cache eviction
 * introduces LRU method. Below we define the cache size (size of cache, size of
 * hash table)
 */
#define CACHE_HASH_BITS 10ULL
#define CACHE_HASH_SIZE (2ULL << CACHE_HASH_BITS)
#define CACHE_SIZE (CACHE_HASH_SIZE * 4ULL)

/**
 * @brief Cache entry containing information about an inode.
 */
struct cache_entry {
    /**
     * Item of LRU list
     */
    struct list_head lru;

    /**
     * Item of hash table
     */
    struct hlist_node hash;

    /**
     * inode ID stored in this entry
     */
    uint64_t inode_id;

    /**
     * device ID for which indoe belongs to
     */
    dev_t device_id;
};

/**
 * @brief inode tracer
 */
struct iotrace_inode_tracer {
    /**
     * Cache hash table
     */
    DECLARE_HASHTABLE(hash_table, CACHE_HASH_BITS);
    /**
     * Cache LRU eviction list
     */
    struct list_head lru_list;
    /**
     * Cache entries storing information about inodes
     */
    struct cache_entry entries[CACHE_SIZE];
};

int iotrace_create_inode_tracer(iotrace_inode_tracer_t *_iotrace_inode,
                                int cpu) {
    int i;
    struct iotrace_inode_tracer *inode_tracer;

    debug();

    *_iotrace_inode = NULL;
    inode_tracer = vzalloc_node(sizeof(*inode_tracer), cpu_to_node(cpu));
    if (!inode_tracer) {
        return -ENOMEM;
    }

    hash_init(inode_tracer->hash_table);
    INIT_LIST_HEAD(&inode_tracer->lru_list);

    /* Initialize LRU list and hash table's nodes*/
    for (i = 0; i < ARRAY_SIZE(inode_tracer->entries); i++) {
        struct cache_entry *entry = &inode_tracer->entries[i];
        list_add(&entry->lru, &inode_tracer->lru_list);
        INIT_HLIST_NODE(&entry->hash);
    }

    *_iotrace_inode = inode_tracer;
    return 0;
}

void iotrace_destroy_inode_tracer(iotrace_inode_tracer_t *iotrace_inode) {
    debug();

    if (*iotrace_inode) {
        vfree(*iotrace_inode);
        *iotrace_inode = NULL;
    }
}

static void _set_hot(iotrace_inode_tracer_t inode_tracer,
                     struct cache_entry *entry) {
    list_del(&entry->lru);
    list_add(&entry->lru, &inode_tracer->lru_list);
}

static struct cache_entry *_get_entry(iotrace_inode_tracer_t inode_tracer) {
    struct cache_entry *entry;

    // No more free entries, we need to evict one
    entry = list_last_entry(&inode_tracer->lru_list, struct cache_entry, lru);

    debug("Remove %llu", entry->inode_id);

    list_del(&entry->lru);
    hash_del(&entry->hash);

    return entry;
}

static void _map(iotrace_inode_tracer_t inode_tracer, struct inode *inode) {
    struct cache_entry *entry = _get_entry(inode_tracer);

    entry->inode_id = inode->i_ino;
    entry->device_id = inode->i_sb->s_dev;

    list_add(&entry->lru, &inode_tracer->lru_list);
    hash_add(inode_tracer->hash_table, &entry->hash, inode->i_ino);

    debug("Map %lu", inode->i_ino);
}

static struct cache_entry *_lookup(iotrace_inode_tracer_t inode_tracer,
                                   struct inode *inode) {
    struct cache_entry *entry = NULL;

    hash_for_each_possible(inode_tracer->hash_table, entry, hash,
                           inode->i_ino) {
        if (inode->i_ino == entry->inode_id &&
            inode->i_sb->s_dev == entry->device_id) {
            debug("Hit %lu", inode->i_ino);
            _set_hot(inode_tracer, entry);
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
    {
        size_t smax = dentry->d_name.len;
        size_t dmax = sizeof(ev.file_name) - 1;
        size_t to_copy = min(smax, dmax);
        memcpy_s(ev.file_name, dmax, dentry->d_name.name, to_copy);
        ev.file_name[dmax] = '\0';
    }

    // TODO (mariuszbarczak) Switch to operate directly on OCTF trace buffer
    return octf_trace_push(trace, &ev, sizeof(ev));
}

void iotrace_trace_inode(struct iotrace_state *state,
                         octf_trace_t trace,
                         iotrace_inode_tracer_t inode_tracer,
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
        entry = _lookup(inode_tracer, this_inode);
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
            _map(inode_tracer, this_inode);
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

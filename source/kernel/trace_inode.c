#include "trace_inode.h"

#include <linux/atomic.h>
#include <linux/dcache.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/fsnotify_backend.h>
#include <linux/hashtable.h>
#include <linux/kallsyms.h>
#include <linux/list.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/version.h>
#include "config.h"
#include "context.h"
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
    /**
     * Filesystem events monitor
     */
    struct fs_monitor *fsm;
};

struct iotrace_group_priv {
    /** Tracer for inodes */
    iotrace_inode_tracer_t inode_tracer;

    /** Allocation cache for marks */
    struct kmem_cache *mark_cache;
};

/** FS events which are collected */
#define IOTRACE_FSNOTIFY_EVENTS \
    (FS_MOVED_FROM | FS_MOVED_TO | FS_CREATE | FS_DELETE_SELF)

/**
 * @brief Operations provided by fsnotify. They are not exported and need to be
 * looked up.
 */
struct fsnotify_backend_ops {
    typeof(&FSNOTIFY_FUN(get_group)) get_group;
    typeof(&FSNOTIFY_FUN(put_group)) put_group;
    typeof(&FSNOTIFY_FUN(alloc_group)) alloc_group;
    typeof(&FSNOTIFY_FUN(destroy_group)) destroy_group;
    typeof(&FSNOTIFY_FUN(init_mark)) init_mark;
    typeof(&FSNOTIFY_FUN(put_mark)) put_mark;
    typeof(&FSNOTIFY_FUN(find_mark)) find_mark;
    typeof(&FSNOTIFY_FUN(IOTRACE_FSNOTIFY_ADD_MARK_NAME))
            IOTRACE_FSNOTIFY_ADD_MARK_NAME;
} static fsnotify_ops;

/** Looks up fsnotify_... function, and evaluates to the result of lookup*/
#define fsnotify_lookup_symbol(f)                                       \
    ({                                                                  \
        fsnotify_ops.f = (void *) kallsyms_lookup_name("fsnotify_" #f); \
        fsnotify_ops.f != NULL ? 0 : -EINVAL;                           \
    })

/** File system events monitor */
struct fs_monitor {
    /** Reference counter */
    refcount_t refcnt;

    /** fsnotify group of this monitor, for receiving fs events */
    struct fsnotify_group *group;
};

/** Update refcounts of group and fsmonitor */
static void _fs_monitor_get(struct fs_monitor *fsm) {
    refcount_inc(&fsm->refcnt);
    fsnotify_ops.get_group(fsm->group);
}

/** Put references of group and fsmonitor */
static void _fs_monitor_put(struct fs_monitor *fsm) {
    if (refcount_dec_and_test(&fsm->refcnt)) {
        debug("Destroying FS monitor");
        fsnotify_ops.destroy_group(fsm->group);
        kfree(fsm);

    } else {
        fsnotify_ops.put_group(fsm->group);
    }
}

/** Callback for freeing group private data */
void _fs_free_group_priv(struct fsnotify_group *group) {
    struct iotrace_group_priv *group_priv;
    debug("Freeing fs_notify_group");

    if (group->private) {
        group_priv = (struct iotrace_group_priv *) group->private;

        kmem_cache_destroy(group_priv->mark_cache);
        kfree(group_priv);
        group->private = NULL;
    }
}

/**
 * Add mark to inode, causing FS events from this
 * inode and child inodes to notify group
 */
static void _fs_add_mark(struct fsnotify_group *group, struct inode *inode) {
    iotrace_inode_tracer_t inode_tracer;
    struct iotrace_group_priv *group_priv;
    struct fs_monitor *fsm;
    struct fsnotify_mark *mark;
    int result;

    group_priv = (struct iotrace_group_priv *) group->private;
    inode_tracer = ((struct iotrace_group_priv *) group->private)->inode_tracer;
    fsm = inode_tracer->fsm;

    /* Find mark belonging to this group in the list of inode marks */
    mark = fsnotify_ops.find_mark(&inode->i_fsnotify_marks, group);

    if (mark) {
        /* Mark already set in this group, nothing to do */
        debug("Mark already set, inode id = %lu", inode->i_ino);

        fsnotify_ops.put_mark(mark);
        return;
    }

    mark = kmem_cache_zalloc(group_priv->mark_cache, GFP_KERNEL);
    fsnotify_ops.init_mark(mark, fsm->group);

    /* All events interest us, in particular EVENT_ON_CHILD */
    mark->mask = ALL_FSNOTIFY_EVENTS;

    result = IOTRACE_FSNOTIFY_ADD_MARK(mark, inode);

    if (result) {
        debug("add_mark error");
        fsnotify_ops.put_mark(mark);
        return;
    }

    fsnotify_ops.put_mark(mark);
    debug("Mark added, inode id = %lu", inode->i_ino);
}

void _trace_file_event(uint64_t devId,
                       uint64_t fileId,
                       uint64_t parentId,
                       iotrace_fs_event_type eventType) {
    uint64_t sid;
    unsigned int cpu;
    octf_trace_t trace;
    struct iotrace_context *context;
    struct iotrace_event_fs_file_event ev = {};

    ev.device_id = devId;
    ev.file_id = fileId;
    ev.parent_id = parentId;
    ev.fs_event_type = eventType;

    context = iotrace_get_context();

    cpu = get_cpu();
    trace = *per_cpu_ptr(context->trace_state.traces, cpu);
    sid = atomic64_inc_return(&context->trace_state.sid);

    iotrace_event_init_hdr(&ev.hdr, iotrace_event_type_fs_file_event, sid,
                           ktime_to_ns(ktime_get()), sizeof(ev));

    octf_trace_push(trace, &ev, sizeof(ev));
    put_cpu();
}

/** Handler code for fs events */
static int _fs_handle_event(struct fsnotify_group *group,
                            struct inode *inode,
                            u32 mask,
                            const void *data,
                            int data_type) {
    struct inode *child_inode;

    switch (data_type) {
    case (FSNOTIFY_EVENT_PATH):
        child_inode = ((const struct path *) data)->dentry->d_inode;
        break;
    case (FSNOTIFY_EVENT_INODE):
        child_inode = (struct inode *) data;
        break;
    case (FSNOTIFY_EVENT_NONE):
        return 0;
    default:
        debug("Unknown event data type in event handler");
        BUG();
        break;
    }

    if (mask & FS_MOVED_FROM & IOTRACE_FSNOTIFY_EVENTS) {
        _trace_file_event(disk_devt(child_inode->i_sb->s_bdev->bd_disk),
                          child_inode->i_ino, inode->i_ino,
                          iotrace_fs_event_move_from);
    }
    if (mask & FS_MOVED_TO & IOTRACE_FSNOTIFY_EVENTS) {
        _trace_file_event(disk_devt(child_inode->i_sb->s_bdev->bd_disk),
                          child_inode->i_ino, inode->i_ino,
                          iotrace_fs_event_move_to);
    }
    if (mask & FS_CREATE) {
        _fs_add_mark(group, child_inode);

        if (mask & IOTRACE_FSNOTIFY_EVENTS) {
            _trace_file_event(disk_devt(child_inode->i_sb->s_bdev->bd_disk),
                              child_inode->i_ino, inode->i_ino,
                              iotrace_fs_event_create);
        }
    }

    if (mask & FS_DELETE_SELF & IOTRACE_FSNOTIFY_EVENTS) {
        /*
         * We have no information here about parent inode - it is in a separate
         * event - FS_DELETE
         */
        _trace_file_event(disk_devt(child_inode->i_sb->s_bdev->bd_disk),
                          child_inode->i_ino, 0, iotrace_fs_event_delete);
    }

    if (mask & FS_OPEN) {
        /* Mark opened files and directories */
        _fs_add_mark(group, child_inode);
    }

    return 0;
}

/* Switch for different kernels */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
static int iotrace_fs_handle_event(struct fsnotify_group *group,
                                   struct inode *inode,
                                   struct fsnotify_mark *inode_mark,
                                   struct fsnotify_mark *vfsmount_mark,
                                   u32 mask,
                                   const void *data,
                                   int data_type,
                                   const unsigned char *file_name,
                                   u32 cookie,
                                   struct fsnotify_iter_info *iter_info)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
static int iotrace_fs_handle_event(struct fsnotify_group *group,
                                   struct inode *inode,
                                   u32 mask,
                                   const void *data,
                                   int data_type,
                                   const unsigned char *file_name,
                                   u32 cookie,
                                   struct fsnotify_iter_info *iter_info)
#else
static int iotrace_fs_handle_event(struct fsnotify_group *group,
                                   struct inode *inode,
                                   u32 mask,
                                   const void *data,
                                   int data_type,
                                   const struct qstr *file_name,
                                   u32 cookie,
                                   struct fsnotify_iter_info *iter_info)
#endif
{
    return _fs_handle_event(group, inode, mask, data, data_type);
}

/** Callback for freeing mark when refcount drops to 0 */
static void iotrace_fs_free_mark(struct fsnotify_mark *mark) {
    struct iotrace_group_priv *group_priv =
            (struct iotrace_group_priv *) mark->group->private;
    kmem_cache_free(group_priv->mark_cache, mark);
}

/**
 * @brief fsmonitor group's ops
 */
static const struct fsnotify_ops fsm_group_ops = {
        .handle_event = iotrace_fs_handle_event,
        .free_mark = iotrace_fs_free_mark,
        .free_group_priv = _fs_free_group_priv,
};

/** Try to get fs monitor, from the the inode_tracer which has it */
struct fs_monitor *_fsm_try_get(void) {
    struct iotrace_context *context = iotrace_get_context();
    struct iotrace_state *state = &context->trace_state;
    iotrace_inode_tracer_t *inode_tracer;

    int i;
    for_each_online_cpu(i) {
        inode_tracer = per_cpu_ptr(state->inode_traces, i);

        if (NULL == *inode_tracer) {
            continue;
        }

        if (NULL == (*inode_tracer)->fsm) {
            continue;
        }

        _fs_monitor_get((*inode_tracer)->fsm);
        return (*inode_tracer)->fsm;
    }

    return NULL;
}

/** Allocate only one fsmonitor */
static void _fsm_init(iotrace_inode_tracer_t inode_tracer) {
    struct fs_monitor *fsm = NULL;
    struct iotrace_group_priv *priv;

    /* Check if FS monitor operations have been resolved */
    if (NULL == fsnotify_ops.get_group) {
        int result = 0;

        result |= fsnotify_lookup_symbol(get_group);
        result |= fsnotify_lookup_symbol(put_group);
        result |= fsnotify_lookup_symbol(alloc_group);
        result |= fsnotify_lookup_symbol(destroy_group);
        result |= fsnotify_lookup_symbol(init_mark);
        result |= fsnotify_lookup_symbol(put_mark);
        result |= fsnotify_lookup_symbol(find_mark);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
        result |= fsnotify_lookup_symbol(add_inode_mark);
#else
        result |= fsnotify_lookup_symbol(add_mark);
#endif

        if (result) {
            printk(KERN_WARNING "Cannot lookup FS monitor's operations\n");
            memset_s(&fsnotify_ops, sizeof(fsnotify_ops), 0);
            return;
        } else {
            printk(KERN_INFO
                   "FS monitor's operations initialized (symbols looked up)\n");
        }
    }

    /* First try get existing FS monitor */
    fsm = _fsm_try_get();
    if (fsm) {
        /* FS monitor already has been created */
        inode_tracer->fsm = fsm;
        return;
    }

    /* Allocate FS monitor */
    fsm = kzalloc(sizeof(*fsm), GFP_KERNEL);
    refcount_set(&fsm->refcnt, 1);

    /* Allocate FS notify group for receiving fs events */
    fsm->group = fsnotify_ops.alloc_group(&fsm_group_ops);

    if (IS_ERR_OR_NULL(fsm->group)) {
        goto ERROR;
    }

    BUG_ON(fsm->group->private);

    priv = kzalloc(sizeof(struct iotrace_group_priv), GFP_KERNEL);
    priv->inode_tracer = inode_tracer;
    priv->mark_cache = kmem_cache_create(
            "fsmark_cache", sizeof(struct fsnotify_mark), 0, 0, NULL);

    fsm->group->private = priv;
    inode_tracer->fsm = fsm;

    debug("FS monitor created");
    return;

ERROR:
    printk(KERN_WARNING "Cannot setup fsnotify backend\n");
    if (fsm) {
        kfree(fsm);
        fsm = NULL;
    }
}

int iotrace_create_inode_tracer(iotrace_inode_tracer_t *_inode_tracer,
                                int cpu) {
    int i;
    struct iotrace_inode_tracer *inode_tracer;

    debug();

    *_inode_tracer = NULL;
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

    /* Initialize FS monitor */
    _fsm_init(inode_tracer);

    *_inode_tracer = inode_tracer;
    return 0;
}

void iotrace_destroy_inode_tracer(iotrace_inode_tracer_t *iotrace_inode) {
    debug();

    if (*iotrace_inode) {
        _fs_monitor_put((*iotrace_inode)->fsm);
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

int _trace_filename(struct iotrace_state *state,
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

/** Handle trace event related to inode */
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

            if (S_ISDIR(parent_inode->i_mode)) {
                if (inode_tracer->fsm) {
                    _fs_add_mark(inode_tracer->fsm->group, parent_inode);
                }
            }
        }

        // Trace dentry name (file or directory name)
        debug("ID = %lu, name = %s", this_inode->i_ino, this_dentry->d_iname);

        result = _trace_filename(
                state, trace, this_inode->i_sb->s_dev, this_inode->i_ino,
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

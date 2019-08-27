/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "procfs.h"
#include <asm/atomic.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include "config.h"
#include "context.h"
#include "iotrace_event.h"
#include "procfs_files.h"
#include "trace.h"
#include "trace_bdev.h"

static inline uint64_t iotrace_page_count(uint64_t size) {
    return (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
}

static const char iotrace_subdir[] = IOTRACE_PROCFS_SUBDIR_NAME;

/* Maximal length of buffer with version information */
static const unsigned version_buffer_max_len = 64;

static void _iotrace_free(struct kref *kref) {
    struct iotrace_proc_file *proc_file =
            container_of(kref, struct iotrace_proc_file, ref);

    vfree(proc_file->trace_ring);
    vfree(proc_file->consumer_hdr);

    proc_file->trace_ring = NULL;
    proc_file->consumer_hdr = NULL;
}

static int _iotrace_open(struct inode *inode, struct file *file, bool writeable)

{
    struct iotrace_proc_file *proc_file = PDE_DATA(inode);
    int result;

    if ((file->f_mode & FMODE_WRITE) && !writeable)
        return -EACCES;

    result = iotrace_attach_client(iotrace_get_context());
    if (result)
        return result;

    kref_get(&proc_file->ref);
    file->private_data = proc_file;
    return 0;
}

static int _iotrace_open_ring(struct inode *inode, struct file *file) {
    return _iotrace_open(inode, file, false);
}

static int _iotrace_open_consumer_hdr(struct inode *inode, struct file *file) {
    return _iotrace_open(inode, file, true);
}

static int _iotrace_release(struct inode *inode, struct file *file) {
    struct iotrace_proc_file *proc_file = PDE_DATA(inode);
    iotrace_detach_client(iotrace_get_context());
    kref_put(&proc_file->ref, _iotrace_free);
    return 0;
}

static long _iotrace_ioctl(struct file *file,
                           unsigned int cmd,
                           unsigned long arg) {
    struct iotrace_proc_file *proc_file = PDE_DATA(file->f_inode);
    int cpu = proc_file->cpu;
    struct iotrace_context *context = iotrace_get_context();
    struct iotrace_cpu_context *cpu_context =
            per_cpu_ptr(context->cpu_context, cpu);
    octf_trace_t handle = *per_cpu_ptr(context->trace_state.traces, cpu);
    int result = -EINVAL;

    switch (cmd) {
    case IOTRACE_IOCTL_WAIT_FOR_TRACES: {
        atomic_set(&cpu_context->waiting_for_trace, 1);

        result = wait_event_interruptible(
                cpu_context->wait_queue,
                (1 == octf_trace_is_almost_full(handle)) ||
                        !atomic_read(&cpu_context->waiting_for_trace));

        atomic_set(&cpu_context->waiting_for_trace, 0);
    } break;

    case IOTRACE_IOCTL_INTERRUPT_WAIT_FOR_TRACES: {
        atomic_set(&cpu_context->waiting_for_trace, 0);
        wake_up(&cpu_context->wait_queue);
        result = 0;
    } break;

    default: { } break; }

    return result;
}

static ssize_t _iotrace_read(struct file *file,
                             char __user *data,
                             size_t size,
                             loff_t *off) {
    return -ENOTSUPP;
}

static ssize_t _iotrace_write(struct file *file,
                              const char __user *data,
                              size_t size,
                              loff_t *off) {
    return -ENOTSUPP;
}

static loff_t _iotrace_llseek(struct file *file, loff_t offset, int whence) {
    return -ENOTSUPP;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 10, 0)
iotrace_vm_fault_t _iotrace_fault(struct vm_area_struct *vma,
                                  struct vm_fault *vmf,
                                  bool trace_ring) {
    struct file *file = vma->vm_file;
#else
iotrace_vm_fault_t _iotrace_fault(struct vm_fault *vmf, bool trace_ring) {
    struct file *file = vmf->vma->vm_file;
#endif
    struct iotrace_proc_file *proc_file = file->private_data;
    void *buf = trace_ring ? proc_file->trace_ring : proc_file->consumer_hdr;
    size_t buf_size =
            trace_ring ? proc_file->trace_ring_size : OCTF_TRACE_HDR_SIZE;
    size_t page_count = iotrace_page_count(buf_size);

    BUILD_BUG_ON(OCTF_TRACE_HDR_SIZE % PAGE_SIZE != 0);

    if (vmf->pgoff >= page_count)
        return VM_FAULT_SIGBUS;

    vmf->page = vmalloc_to_page(buf + (vmf->pgoff << PAGE_SHIFT));
    if (!vmf->page)
        return -EACCES;

    get_page(vmf->page);
    return VM_FAULT_MAJOR;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 10, 0)
int _iotrace_fault_trace_ring(struct vm_area_struct *vma,
                              struct vm_fault *vmf) {
    return _iotrace_fault(vma, vmf, true);
}

int _iotrace_fault_consumer_hdr(struct vm_area_struct *vma,
                                struct vm_fault *vmf) {
    return _iotrace_fault(vma, vmf, false);
}
#else
iotrace_vm_fault_t _iotrace_fault_trace_ring(struct vm_fault *vmf) {
    return _iotrace_fault(vmf, true);
}
iotrace_vm_fault_t _iotrace_fault_consumer_hdr(struct vm_fault *vmf) {
    return _iotrace_fault(vmf, false);
}
#endif

static const struct vm_operations_struct _iotrace_vm_ops_trace_ring = {
        .fault = _iotrace_fault_trace_ring};

static const struct vm_operations_struct _iotrace_vm_ops_consumer_hdr = {
        .fault = _iotrace_fault_consumer_hdr};

static int _iotrace_mmap_trace_ring(struct file *file,
                                    struct vm_area_struct *vma) {
    /* do not allow write-mapping */
    if (vma->vm_flags & VM_WRITE)
        return -EPERM;

    /* do not allow mapping for files opened with write permissions, to assure
     * that mapping is not mprotect-ed to RW later on */
    if (file->f_mode & FMODE_WRITE)
        return -EPERM;

    vma->vm_ops = &_iotrace_vm_ops_trace_ring;

    return 0;
}

static int _iotrace_mmap_consumer_hdr(struct file *file,
                                      struct vm_area_struct *vma) {
    vma->vm_ops = &_iotrace_vm_ops_consumer_hdr;
    return 0;
}

static const struct file_operations _iotrace_trace_ring_fops = {
        .owner = THIS_MODULE,
        .open = _iotrace_open_ring,
        .write = _iotrace_write,
        .read = _iotrace_read,
        .unlocked_ioctl = _iotrace_ioctl,
        .llseek = _iotrace_llseek,
        .release = _iotrace_release,
        .mmap = _iotrace_mmap_trace_ring,
};

static const struct file_operations _iotrace_consumer_hdr_fops = {
        .owner = THIS_MODULE,
        .open = _iotrace_open_consumer_hdr,
        .write = _iotrace_write,
        .read = _iotrace_read,
        .llseek = _iotrace_llseek,
        .release = _iotrace_release,
        .mmap = _iotrace_mmap_consumer_hdr,
};

/* Function writing specific data to buffer with semantics similar to snprintf
 *  - input size must not be exceeded and output must be NULL - terminated.
 *  Unlike standard snprintf it can return negative error code and might
 *  return -ENOSPC instead of truncating the string. */
typedef int (*iotrace_snprintf_t)(char *buf, size_t size);

/* Function reading specific data from buffer and setting it in io tracer.
 * Input buffer is guaranteed to be NULL terminated. In case of error nagative
 * erorr code is returned. */
typedef int (*iotrace_sscanf_t)(const char *buf);

/**
 * @brief Generic write handler for iotrace management procfs files
 *
 * @param[in] file file object associated with this procfs entry
 * @param[out] ubuf user pointer to input buffer
 * @param[in] count ubuf size
 * @param[in/out] ppos position in file before/after write operation
 * @param[in] handler operation handler operating on null-terminated strings
 *
 * @retval number of bytes read from @ubuf
 */
static ssize_t iotrace_mngt_write(struct file *file,
                                  const char __user *ubuf,
                                  size_t count,
                                  loff_t *ppos,
                                  iotrace_sscanf_t sscanf_handler) {
    char *buf;
    int result;
    size_t len;

    if (*ppos > 0 || count >= PATH_MAX)
        return -EFAULT;

    if (!IOTRACE_ACCESS_OK(VERIFY_READ, ubuf, count))
        return -EFAULT;

    /* one byte more to put terminating 0 there */
    buf = vzalloc(count + 1);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, ubuf, count)) {
        vfree(buf);
        return -EFAULT;
    }

    len = strnlen(buf, count);
    if (len == 0) {
        vfree(buf);
        return -EINVAL;
    }

    if (buf[len - 1] == '\n')
        buf[len - 1] = '\0';
    else
        buf[len] = '\0';

    result = sscanf_handler(buf);

    vfree(buf);

    if (result)
        return result;

    *ppos = len;
    return len;
}

/**
 * @brief Generic read handler for iotrace management procfs files.
 *
 * @param[in] file file object associated with this procfs entry
 * @param[out] ubuf user pointer to output buffer
 * @param[in] ubuf_size ubuf size
 * @param[in/out] ppos position in file before/after read operation
 * @param[in] data_size size of internal string representation of requested
 *			  data, excluding terminating string
 * @param[in] handler operation handler operating on null-terminated strings
 *
 * @retval number of bytes written @ubuf
 */
static ssize_t iotrace_mngt_read(struct file *file,
                                 char __user *ubuf,
                                 size_t ubuf_size,
                                 loff_t *ppos,
                                 size_t data_size,
                                 iotrace_snprintf_t snprintf_handler) {
    size_t count;
    void *buf;
    int result;

    if (*ppos > 0)
        return 0;

    if (!IOTRACE_ACCESS_OK(VERIFY_WRITE, ubuf, ubuf_size))
        return -EACCES;

    /* allocate internal buffer on byte larger to contain terminating NULL,
     * which is not copied back to user */
    count = min(data_size, ubuf_size) + 1;

    buf = vzalloc(count);
    if (!buf)
        return -ENOMEM;

    result = snprintf_handler(buf, count);
    if (result < 0) {
        vfree(buf);
        return result;
    } else if (result >= count) {
        vfree(buf);
        return -ENOSPC;
    }
    count = result;

    BUG_ON(count > ubuf_size);

    /* copy list of devices, excluding terminating NULL */
    if (count != 0 && copy_to_user(ubuf, buf, count)) {
        vfree(buf);
        return -EFAULT;
    }

    *ppos = count;

    vfree(buf);
    return count;
}

static int _add_dev_scanf(const char *buf) {
    return iotrace_bdev_add(&iotrace_get_context()->bdev, buf);
}

/**
 * @brief Write handler for file used to add device to tracer list
 *
 * @param[in] file file object associated with this procfs entry
 * @param[out] ubuf user pointer to input buffer
 * @param[in] count ubuf size
 * @param[out] ppos position in file after write operation is completed
 *
 * @retval number of bytes read from @ubuf
 */
static ssize_t add_dev_write(struct file *file,
                             const char __user *ubuf,
                             size_t count,
                             loff_t *ppos) {
    return iotrace_mngt_write(file, ubuf, count, ppos, _add_dev_scanf);
}

static int _del_dev_scanf(const char *buf) {
    return iotrace_bdev_remove(&iotrace_get_context()->bdev, buf);
}

/**
 * @brief Write handler for file used to remove device to tracer list
 *
 * @param[in] file file object associated with this procfs entry
 * @param[out] ubuf user pointer to input buffer
 * @param[in] count ubuf size
 * @param[out] ppos position in file after write operation is completed
 *
 * @retval number of bytes read from @ubuf
 */
static ssize_t del_dev_write(struct file *file,
                             const char __user *ubuf,
                             size_t count,
                             loff_t *ppos) {
    return iotrace_mngt_write(file, ubuf, count, ppos, _del_dev_scanf);
}

static int _list_dev_snprintf(char *buf, size_t size) {
    char devices[SATRACE_MAX_DEVICES][DISK_NAME_LEN];
    unsigned dev_count;
    int result;
    int pos, idx;
    size_t bytes_left, bytes_copied;

    result = iotrace_bdev_list(&iotrace_get_context()->bdev, devices);
    if (result < 0)
        return result;
    dev_count = result;

    bytes_left = size;
    pos = 0;
    buf[0] = '\0';
    for (idx = 0; idx < dev_count; idx++) {
        bytes_copied = snprintf(buf + pos, bytes_left, "%s\n", devices[idx]);
        bytes_copied = min(bytes_copied, bytes_left);

        bytes_left -= bytes_copied;
        pos += bytes_copied;

        if (!bytes_left)
            break;
    }

    /* return size excluding terminating NULL */
    return pos;
}

/**
 * @brief Read handler for file used to report list of traced devices
 *
 * @param[in] file file object associated with this procfs entry
 * @param[out] ubuf user pointer to output buffer
 * @param[in] count ubuf size
 * @param[out] ppos position in file after read operation is completed
 *
 * @retval number of bytes written to @ubuf
 */
static ssize_t list_dev_read(struct file *file,
                             char __user *ubuf,
                             size_t count,
                             loff_t *ppos) {
    return iotrace_mngt_read(file, ubuf, count, ppos,
                             SATRACE_MAX_DEVICES * DISK_NAME_LEN,
                             _list_dev_snprintf);
}

static int _version_snprintf(char *buf, size_t size) {
    return snprintf(buf, size, "%d\n%d\n%016llX\n", IOTRACE_EVENT_VERSION_MAJOR,
                    IOTRACE_EVENT_VERSION_MINOR, IOTRACE_MAGIC);
}

/**
 * @brief Read handler for file used to report version of kernel module
 *
 * @param[in] file file object associated with this procfs entry
 * @param[out] ubuf user pointer to output buffer
 * @param[in] count ubuf size
 * @param[out] ppos position in file after read operation is completed
 *
 * @retval number of bytes written to @ubuf
 */
static ssize_t get_version(struct file *file,
                           char __user *ubuf,
                           size_t count,
                           loff_t *ppos) {
    return iotrace_mngt_read(file, ubuf, count, ppos, version_buffer_max_len,
                             _version_snprintf);
}

static const size_t size_file_max_count = 10;

static int _size_snprintf(char *buf, size_t buf_size) {
    uint64_t size = iotrace_get_buffer_size(iotrace_get_context());

    return snprintf(buf, buf_size, "%llu", size);
}

static ssize_t size_read(struct file *file,
                         char __user *ubuf,
                         size_t count,
                         loff_t *ppos) {
    return iotrace_mngt_read(file, ubuf, count, ppos, size_file_max_count,
                             _size_snprintf);
}

static int _buffer_size_sscanf(const char *buf) {
    int result;
    uint64_t size;

    result = kstrtou64(buf, 10, &size);
    if (result)
        return result;

    return iotrace_init_buffers(iotrace_get_context(), size);
}

static ssize_t size_write(struct file *file,
                          const char __user *ubuf,
                          size_t count,
                          loff_t *ppos) {
    return iotrace_mngt_write(file, ubuf, count, ppos, _buffer_size_sscanf);
}

/* device management files ops */
static struct file_operations add_dev_ops = {.owner = THIS_MODULE,
                                             .write = add_dev_write};
static struct file_operations del_dev_ops = {.owner = THIS_MODULE,
                                             .write = del_dev_write};
static struct file_operations list_dev_ops = {
        .owner = THIS_MODULE,
        .read = list_dev_read,
};
static struct file_operations get_version_ops = {
        .owner = THIS_MODULE,
        .read = get_version,
};
static struct file_operations size_ops = {
        .owner = THIS_MODULE,
        .write = size_write,
        .read = size_read,
};

/**
 * @brief Initialize iotrace directory in /proc
 *
 * @retval 0 Directory created successfully
 * @retval non-zero Error code
 */
static int iotrace_procfs_mngt_init(void) {
    struct proc_dir_entry *dir, *ent;
    unsigned i;

    struct {
        char *name;
        struct file_operations *ops;
        umode_t mode;
    } entries[] = {
            {
                    .name = IOTRACE_PROCFS_DEVICES_FILE_NAME,
                    .ops = &list_dev_ops,
                    .mode = S_IRUSR,
            },
            {
                    .name = IOTRACE_PROCFS_ADD_DEVICE_FILE_NAME,
                    .ops = &add_dev_ops,
                    .mode = S_IWUSR,
            },
            {
                    .name = IOTRACE_PROCFS_REMOVE_DEVICE_FILE_NAME,
                    .ops = &del_dev_ops,
                    .mode = S_IWUSR,
            },
            {
                    .name = IOTRACE_PROCFS_VERSION_FILE_NAME,
                    .ops = &get_version_ops,
                    .mode = S_IRUSR,
            },
            {
                    .name = IOTRACE_PROCFS_SIZE_FILE_NAME,
                    .ops = &size_ops,
                    .mode = S_IRUSR | S_IWUSR,
            },
    };
    size_t num_entries = sizeof(entries) / sizeof(entries[0]);

    /* create iotrace directory */
    dir = proc_mkdir(iotrace_subdir, NULL);
    if (!dir) {
        printk(KERN_ERR "Cannot create /proc/%s\n", iotrace_subdir);
        return -ENOENT;
    }

    /* create iotrace management file interfaces */
    for (i = 0; i < num_entries; i++) {
        ent = proc_create(entries[i].name, entries[i].mode, dir,
                          entries[i].ops);
        if (!ent) {
            printk(KERN_ERR "Cannot create /proc/%s/%s\n", iotrace_subdir,
                   entries[i].name);
            break;
        }
    }

    if (i < num_entries) {
        /* error */
        proc_remove(dir);
        return -EINVAL;
    }

    return 0;
}

/* Allocate buffer for traces */
int iotrace_procfs_trace_file_alloc(struct iotrace_proc_file *proc_file,
                                    uint64_t size,
                                    int cpu) {
    void *buffer;
    uint64_t allocation_size;

    if (size < OCTF_TRACE_HDR_SIZE)
        return -ENOSPC;

    /* make consumer_hdr and trace ring buffer sizes add up to requested total
     */
    size -= OCTF_TRACE_HDR_SIZE;
    allocation_size = iotrace_page_count(size) << PAGE_SHIFT;

    if (proc_file->trace_ring && proc_file->trace_ring_size == size)
        return 0;

    buffer = vzalloc_node(allocation_size, cpu_to_node(cpu));
    if (!buffer)
        return -ENOMEM;

    vfree(proc_file->trace_ring);

    proc_file->cpu = cpu;
    proc_file->trace_ring = buffer;
    proc_file->trace_ring_size = size;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 32)
    proc_set_size(proc_file->trace_ring_entry, size);
#else
    proc_file->trace_ring_entry->size = size;
#endif

    return 0;
}

/**
 * @brief Initialize trace file in iotrace procfs dir
 *
 * @param[out] proc_file Object describing procfs file
 * @param[in] cpu CPU associated with this file
 *
 * @retval 0 File created successfully
 * @retval non-zero Error code
 */
static int iotrace_procfs_trace_file_init(struct iotrace_proc_file *proc_file,
                                          unsigned cpu) {
    int result;
    char trace_ring_path[128];
    char consumer_hdr_path[128];

    result = snprintf(trace_ring_path, sizeof(trace_ring_path),
                      "%s/" IOTRACE_PROCFS_TRACE_FILE_PREFIX "%u",
                      iotrace_subdir, cpu);
    if (result < 0 || result >= sizeof(trace_ring_path))
        return -ENOSPC;

    result = snprintf(consumer_hdr_path, sizeof(consumer_hdr_path),
                      "%s/" IOTRACE_PROCFS_CONSUMER_HDR_FILE_PREFIX "%u",
                      iotrace_subdir, cpu);
    if (result < 0 || result >= sizeof(consumer_hdr_path))
        return -ENOSPC;

    kref_init(&proc_file->ref);

    /* Initialize wait_queue */
    init_waitqueue_head(&(proc_file->poll_wait_queue));

    /* Allocate consumer_hdr buffer */
    proc_file->consumer_hdr = vmalloc_user(sizeof(octf_trace_hdr_t));
    if (!proc_file->consumer_hdr)
        return -ENOMEM;

    /* Create trace ring buffer read only file */
    proc_file->trace_ring_entry =
            proc_create_data(trace_ring_path, S_IRUSR, NULL,
                             &_iotrace_trace_ring_fops, proc_file);
    if (!proc_file->trace_ring_entry) {
        vfree(proc_file->consumer_hdr);
        return -ENOENT;
    }

    /* Create consumer_hdr buffer RW file */
    proc_file->consumer_hdr_entry =
            proc_create_data(consumer_hdr_path, S_IRUSR | S_IWUSR, NULL,
                             &_iotrace_consumer_hdr_fops, proc_file);
    if (!proc_file->consumer_hdr_entry) {
        vfree(proc_file->consumer_hdr);
        proc_remove(proc_file->trace_ring_entry);
        proc_file->trace_ring_entry = NULL;
        return -ENOENT;
    }

    /* Set trace file sizes. Ring buffer size is going to be set later per
     * user request, so now just initializing it to 0. Consumer header is
     * of fixed size and already allocated, so this one is set here. */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 32)
    proc_set_size(proc_file->trace_ring_entry, 0);
    proc_set_size(proc_file->consumer_hdr_entry, OCTF_TRACE_HDR_SIZE);
#else
    proc_file->trace_ring_entry->size = 0;
    proc_file->consumer_hdr_entry->size = OCTF_TRACE_HDR_SIZE;
#endif

    proc_file->inited = true;

    return 0;
}

/**
 * @brief Dereference trace file in iotrace procfs dir
 *
 * @param[out] proc_file Object describing procfs file
 */
void iotrace_procfs_trace_file_deinit(struct iotrace_proc_file *proc_file) {
    if (!proc_file->inited)
        return;

    kref_put(&proc_file->ref, _iotrace_free);
    proc_file->inited = false;
}

/**
 * @brief Deinitialize iotrace procfs directory tree
 */
void iotrace_procfs_mngt_deinit(void) {
    if (remove_proc_subtree(iotrace_subdir, NULL))
        WARN(true, "/proc/%s does not exist\n", iotrace_subdir);
}

/**
 * @brief Initialize directories and files in /proc
 *
 * Procfs files are used for managing IO tracing and accessing trace buffers
 * from userspace. Files are located in IOTRACE_PROCFS_DIR directory.
 *
 * @param iotrace iotrace context
 *
 * @retval 0 Files initialized successfully
 * @retval non-zero Error code
 */
int iotrace_procfs_init(struct iotrace_context *iotrace) {
    unsigned i;
    int result = 0;

    /* For each cpu (and thus trace file) allocate atomic flag */
    iotrace->cpu_context = alloc_percpu(struct iotrace_cpu_context);
    if (!iotrace->cpu_context) {
        result = -ENOMEM;
        goto error;
    }

    result = iotrace_procfs_mngt_init();
    if (result)
        goto error;

    for_each_online_cpu(i) {
        struct iotrace_cpu_context *cpu_context =
                per_cpu_ptr(iotrace->cpu_context, i);
        init_waitqueue_head(&cpu_context->wait_queue);

        result = iotrace_procfs_trace_file_init(&cpu_context->proc_files, i);
        if (result) {
            printk(KERN_ERR
                   "Failed to register procfs trace "
                   "buffer file\n");
            break;
        }
    }

    if (result) {
        for_each_online_cpu(i) {
            struct iotrace_cpu_context *cpu_context =
                    per_cpu_ptr(iotrace->cpu_context, i);

            iotrace_procfs_trace_file_deinit(&cpu_context->proc_files);
        }
        iotrace_procfs_mngt_deinit();
        goto error;
    }

    return 0;

error:
    free_percpu(iotrace->cpu_context);
    return result;
}

/**
 * @brief Deinitialize all previously created procfs directories and files
 *
 * @param iotrace tracing instance context
 *
 */
void iotrace_procfs_deinit(struct iotrace_context *iotrace) {
    unsigned i;

    for_each_online_cpu(i) {
        struct iotrace_cpu_context *cpu_context =
                per_cpu_ptr(iotrace->cpu_context, i);

        iotrace_procfs_trace_file_deinit(&cpu_context->proc_files);
    }

    free_percpu(iotrace->cpu_context);
    iotrace_procfs_mngt_deinit();
}

/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/version.h>
#include "context.h"
#include "procfs.h"
#include "trace_bdev.h"
#include "iotrace_event.h"
#include "procfs_files.h"
#include "context.h"
#include "trace.h"

static const char iotrace_subdir[] = IOTRACE_PROCFS_SUBDIR_NAME;

static void _iotrace_free(struct kref *kref)
{
	struct iotrace_proc_file *proc_file =
		container_of(kref, struct iotrace_proc_file, ref);

	vfree(proc_file->trace_ring);
	vfree(proc_file->consumer_hdr);

	proc_file->trace_ring = NULL;
	proc_file->consumer_hdr = NULL;
}

static int _iotrace_open(struct inode *inode, struct file *file)
{
	struct iotrace_proc_file *proc_file = PDE_DATA(inode);
	int result;

	result = iotrace_attach_client(iotrace_get_context());
	if (result)
		return result;

	kref_get(&proc_file->ref);
	file->private_data = proc_file;
	return 0;
}

static int _iotrace_release(struct inode *inode, struct file *file)
{
	struct iotrace_proc_file *proc_file = PDE_DATA(inode);
	iotrace_detach_client(iotrace_get_context());
	kref_put(&proc_file->ref, _iotrace_free);
	return 0;
}

static ssize_t _iotrace_read(struct file *file, char __user *data, size_t size,
			      loff_t *off)
{
	return -ENOTSUPP;
}

static unsigned int _iotrace_poll(struct file *file, poll_table *wait)
{
	struct iotrace_proc_file *proc_file = file->private_data;
	octf_trace_t handle = *per_cpu_ptr(
		iotrace_get_context()->trace_state.traces, smp_processor_id());

	/* Register in poll wait_queue */
	poll_wait(file, &proc_file->poll_wait_queue, wait);

	if (!octf_trace_is_empty(handle))
		return POLLIN | POLLRDNORM;

	/* No events ready */
	return 0;
}

static ssize_t _iotrace_write(struct file *file, const char __user *data,
			       size_t size, loff_t *off)
{
	return -ENOTSUPP;
}

static loff_t _iotrace_llseek(struct file *file, loff_t offset, int whence)
{
	return -ENOTSUPP;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 10, 0)
int _iotrace_fault(struct vm_area_struct *vma, struct vm_fault *vmf,
		    bool trace_ring)
#else
int _iotrace_fault(struct vm_fault *vmf, bool trace_ring)
#endif
{
	struct file *file = vma->vm_file;
	struct iotrace_proc_file *proc_file = file->private_data;
	void *buf =
		trace_ring ? proc_file->trace_ring : proc_file->consumer_hdr;
	size_t buf_size =
		trace_ring ? proc_file->trace_ring_size : OCTF_TRACE_HDR_SIZE;
	size_t size = (buf_size + PAGE_SIZE - 1) >> PAGE_SHIFT;

	if (vmf->pgoff >= size)
		return VM_FAULT_SIGBUS;

	vmf->page = vmalloc_to_page(buf + (vmf->pgoff << PAGE_SHIFT));
	get_page(vmf->page);

	return 0;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 10, 0)
int _iotrace_fault_trace_ring(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	return _iotrace_fault(vma, vmf, true);
}

int _iotrace_fault_consumer_hdr(struct vm_area_struct *vma,
				 struct vm_fault *vmf)
{
	return _iotrace_fault(vma, vmf, false);
}
#else
int _iotrace_fault_trace_ring(struct vm_fault *vmf)
{
	return _iotrace_fault(vmf, true);
}
int _iotrace_fault_consumer_hdr(struct vm_fault *vmf)
{
	return _iotrace_fault(vmf, false);
}
#endif

static const struct vm_operations_struct _iotrace_vm_ops_trace_ring = {
	.fault = _iotrace_fault_trace_ring
};

static const struct vm_operations_struct _iotrace_vm_ops_consumer_hdr = {
	.fault = _iotrace_fault_consumer_hdr
};

static int _iotrace_mmap_trace_ring(struct file *file,
				     struct vm_area_struct *vma)
{
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
				       struct vm_area_struct *vma)
{
	vma->vm_ops = &_iotrace_vm_ops_consumer_hdr;
	return 0;
}

static const struct file_operations _iotrace_trace_ring_fops = {
	.owner = THIS_MODULE,
	.open = _iotrace_open,
	.write = _iotrace_write,
	.read = _iotrace_read,
	.llseek = _iotrace_llseek,
	.release = _iotrace_release,
	.mmap = _iotrace_mmap_trace_ring,
	.poll = _iotrace_poll,
};

static const struct file_operations _iotrace_consumer_hdr_fops = {
	.owner = THIS_MODULE,
	.open = _iotrace_open,
	.write = _iotrace_write,
	.read = _iotrace_read,
	.llseek = _iotrace_llseek,
	.release = _iotrace_release,
	.mmap = _iotrace_mmap_consumer_hdr,
	.poll = _iotrace_poll,
};

/**
 * @brief Internal enumeration to distinguish management operations on device
 *	  list
 */
enum iotrace_dev_mng_op { iotrace_dev_add, iotrace_dev_remove };

/**
 * @brief Generic write handler for procfs files used to add/remove device
 *	  from tracer list.
 *
 * @param[in] file file object associated with this procfs entry
 * @param[out] ubuf user pointer to input buffer
 * @param[in] count ubuf size
 * @param[out] ppos position in file after write operation is completed
 * @param[in] op operation type (add/remove)
 *
 * @retval number of bytes read from @ubuf
 */
static ssize_t dev_mng_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos, enum iotrace_dev_mng_op op)
{
	char *buf;
	int result;
	size_t len;

	if (*ppos > 0 || count > PATH_MAX)
		return -EFAULT;

	buf = vzalloc(count);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, count)) {
		vfree(buf);
		return -EFAULT;
	}

	len = strnlen(buf, PATH_MAX);
	if (len >= PATH_MAX || len == 0) {
		vfree(buf);
		return -ENOSPC;
	}

	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	switch (op) {
	case iotrace_dev_add:
		result = iotrace_bdev_add(&iotrace_get_context()->bdev, buf);
		break;
	case iotrace_dev_remove:
		result = iotrace_bdev_remove(&iotrace_get_context()->bdev, buf);
		break;
	default:
		BUG();
	}

	vfree(buf);

	if (result)
		return result;

	*ppos = len;
	return len;
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
static ssize_t add_dev_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	return dev_mng_write(file, ubuf, count, ppos, iotrace_dev_add);
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
static ssize_t del_dev_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	return dev_mng_write(file, ubuf, count, ppos, iotrace_dev_remove);
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
static ssize_t list_dev_read(struct file *file, char __user *ubuf, size_t count,
			     loff_t *ppos)
{
	char devices[SATRACE_MAX_DEVICES][DISK_NAME_LEN];
	unsigned dev_count;
	char *buf;
	int result;
	int pos, idx;

	if (*ppos > 0)
		return 0;

	count = min(sizeof(devices), count);

	buf = vzalloc(count);
	if (!buf)
		return -ENOMEM;

	result = iotrace_bdev_list(&iotrace_get_context()->bdev, devices);
	if (result < 0) {
		vfree(buf);
		return result;
	}
	dev_count = result;

	for (pos = 0, idx = 0;
	     idx < dev_count && pos + strlen(devices[idx]) + 1 < count - pos;
	     idx++) {
		pos += snprintf(buf + pos, count - pos, "%s\n", devices[idx]);
	}

	if (copy_to_user(ubuf, buf, pos + 1)) {
		vfree(buf);
		return -EFAULT;
	}

	*ppos = pos;

	vfree(buf);
	return pos;
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
static ssize_t get_version(struct file *file, char __user *ubuf, size_t count,
			   loff_t *ppos)
{
	int pos;
	struct iotrace_context *context;

	if (*ppos > 0)
		return 0;

	context = iotrace_get_context();
	pos = min(context->version_buff_size, (int)count);

	if (copy_to_user(ubuf, context->version_buff, pos)) {
		return -EFAULT;
	}

	*ppos = pos;
	return pos;
}

static const size_t size_file_max_count = 10;

static ssize_t size_read(struct file *file, char __user *ubuf, size_t count,
			 loff_t *ppos)
{
	int pos;
	char buf[size_file_max_count];
	uint64_t size = iotrace_get_buffer_size(iotrace_get_context());

	if (*ppos > 0)
		return 0;

	pos = snprintf(buf, sizeof(buf), "%llu", size);
	if (pos < 0 || pos >= sizeof(buf))
		return pos < 0 ? pos : -ENOSPC;

	if (copy_to_user(ubuf, buf, pos + 1))
		return -EFAULT;

	*ppos = pos + 1;
	return pos + 1;
}
static ssize_t size_write(struct file *file, const char __user *ubuf,
			  size_t count, loff_t *ppos)
{
	char *buf;
	int result;
	size_t len;
	unsigned long long size;

	if (*ppos > 0 || count > size_file_max_count)
		return -EFAULT;

	buf = vzalloc(count);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, count)) {
		vfree(buf);
		return -EFAULT;
	}

	len = strnlen(buf, size_file_max_count);
	if (len == size_file_max_count || len == 0) {
		vfree(buf);
		return -ENOSPC;
	}

	result = kstrtou64(buf, 10, &size);

	vfree(buf);

	if (result)
		return result;

	result = iotrace_init_buffers(iotrace_get_context(), size);
	if (result)
		return result;

	return len;
}

/* device management files ops */
static struct file_operations add_dev_ops = { .owner = THIS_MODULE,
					      .write = add_dev_write };
static struct file_operations del_dev_ops = { .owner = THIS_MODULE,
					      .write = del_dev_write };
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
static int iotrace_procfs_mngt_init(void)
{
	struct proc_dir_entry *dir, *ent;
	unsigned i;

	struct {
		char *name;
		struct file_operations *ops;
	} entries[] = {
		{ .name = IOTRACE_PROCFS_DEVICES_FILE_NAME, .ops = &list_dev_ops },
		{ .name = IOTRACE_PROCFS_ADD_DEVICE_FILE_NAME, .ops = &add_dev_ops },
		{ .name = IOTRACE_PROCFS_REMOVE_DEVICE_FILE_NAME,
		  .ops = &del_dev_ops },
		{ .name = IOTRACE_PROCFS_VERSION_FILE_NAME,
		  .ops = &get_version_ops },
		{ .name = IOTRACE_PROCFS_SIZE_FILE_NAME, .ops = &size_ops },
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
		ent = proc_create(entries[i].name, 0600, dir, entries[i].ops);
		if (!ent) {
			printk(KERN_ERR "Cannot create /proc/%s/%s\n",
			       iotrace_subdir, entries[i].name);
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
				    uint64_t size)
{
	void *buffer;

	if (size < OCTF_TRACE_HDR_SIZE)
		return -ENOSPC;

	/* make consumer_hdr and trace ring buffer sizes add up to requested total */
	size -= OCTF_TRACE_HDR_SIZE;

	if (proc_file->trace_ring && proc_file->trace_ring_size == size)
		return 0;

	buffer = vmalloc_user(size);
	if (!buffer)
		return -ENOMEM;

	vfree(proc_file->trace_ring);

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
					  unsigned cpu)
{
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
void iotrace_procfs_trace_file_deinit(struct iotrace_proc_file *proc_file)
{
	if (!proc_file->inited)
		return;

	kref_put(&proc_file->ref, _iotrace_free);
	proc_file->inited = false;
}

/**
 * @brief Deinitialize iotrace procfs directory tree
 */
void iotrace_procfs_mngt_deinit(void)
{
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
int iotrace_procfs_init(struct iotrace_context *iotrace)
{
	unsigned i;
	int result = 0;

	iotrace->proc_files = alloc_percpu(struct iotrace_proc_file);
	if (!iotrace->proc_files)
		return -ENOMEM;

	result = iotrace_procfs_mngt_init();
	if (result)
		goto error;

	for_each_online_cpu (i) {
		result = iotrace_procfs_trace_file_init(
			per_cpu_ptr(iotrace->proc_files, i), i);
		if (result) {
			printk(KERN_ERR "Failed to register procfs trace "
					"buffer file\n");
			break;
		}
	}

	if (result) {
		for_each_online_cpu (i) {
			iotrace_procfs_trace_file_deinit(
				per_cpu_ptr(iotrace->proc_files, i));
		}
		iotrace_procfs_mngt_deinit();
		goto error;
	}

	return 0;

error:
	free_percpu(iotrace->proc_files);
	return result;
}

/**
 * @brief Deinitialize all previously created procfs directories and files
 *
 * @param iotrace tracing instance context
 *
 */
void iotrace_procfs_deinit(struct iotrace_context *iotrace)
{
	unsigned i;

	for_each_online_cpu (i)
		iotrace_procfs_trace_file_deinit(
			per_cpu_ptr(iotrace->proc_files, i));

	free_percpu(iotrace->proc_files);

	iotrace_procfs_mngt_deinit();
}

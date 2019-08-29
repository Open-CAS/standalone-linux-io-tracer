#ifndef TRACE_INODE_H_
#define TRACE_INODE_H_

#include "trace.h"

/**
 * @file
 *
 * @brief inodes tracer
 *
 * It provides tracing of following inodes properties:
 * 1. Inode's name
 * 2. Inode's ID
 * 3. Inode's parent ID
 * 4. Inode's device ID
 *
 * - Each element of inode's full path is added as a separate FilenameEvent
 * - Each inode's FilenameEvent is usually (see below) added only once (per
 * cpu).
 * - Due to internal cache limitations, some rarely accesed inodes' filenames
 * may be traced more than once
 *
 * For Inode's names, we keep an internal cache with information if given inode
 * name has been already traced. If it hasn't, the inode name is traced.
 */

/*
 * Forward declarations
 */
struct iotrace_inode_tracer;
struct inode;
struct iotrace_state;

/**
 * Handle of inodes tracer instance
 */
typedef struct iotrace_inode_tracer *iotrace_inode_tracer_t;

/**
 * @brief Creates inode tracer instance
 *
 * @note inode tracer doesn't guaranty thread safety. If you intend to use it
 * across many threads, you need to ensure synchronization yourself
 *
 * @param[out] inode_tracer Handle of created inodes tracer instance
 * @param cpu CPU on which inode tracer will be running
 *
 * @return Operation result
 * @retval 0 - inode tracer created successfully
 * @retval Non-zero error while creating inode tracer
 */
int iotrace_create_inode_tracer(iotrace_inode_tracer_t *inode_tracer, int cpu);

/**
 * @brief Destroys inode tracer
 *
 * @param[in,out] iotrace_inode inode tracer to be destroyed
 */
void iotrace_destroy_inode_tracer(iotrace_inode_tracer_t *inode_tracer);

/**
 * @brief Traces inode
 *
 * @param state Trace state
 * @param trace Circular trace buffer
 * @param inode_tracer inode tracer
 * @param inode inode to be traced
 */
void iotrace_trace_inode(struct iotrace_state *state,
                         octf_trace_t trace,
                         iotrace_inode_tracer_t inode_tracer,
                         struct inode *inode);

#endif  // TRACE_INODE_H_

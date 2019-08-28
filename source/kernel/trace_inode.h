#ifndef TRACE_INODE_H_
#define TRACE_INODE_H_

#include "trace.h"

struct inode;
struct iotrace_state;

typedef struct iotrace_inode *iotrace_inode_t;

int iotrace_inode_create(iotrace_inode_t *iotrace_inode, int cpu);

void iotrace_inode_destroy(iotrace_inode_t *iotrace_inode);

void iotrace_inode_trace(struct iotrace_state *state,
                         octf_trace_t trace,
                         iotrace_inode_t iotrace_inode,
                         struct inode *inode);

#endif  // TRACE_INODE_H_

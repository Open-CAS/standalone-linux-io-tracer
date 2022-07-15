/*
 * Copyright(c) 2012-2020 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_USERSPACE_IOTRACE_BPF_COMMON_H_
#define SOURCE_USERSPACE_IOTRACE_BPF_COMMON_H_

#define MINORBITS 20
#define MINORMASK ((1U << MINORBITS) - 1)
#define MAJOR(dev) ((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev) ((unsigned int) ((dev) &MINORMASK))
#define MKDEV(ma, mi) (((ma) << MINORBITS) | (mi))

#endif /* SOURCE_USERSPACE_IOTRACE_BPF_COMMON_H_ */

/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_KERNEL_INTERNAL_DSS_H
#define SOURCE_KERNEL_INTERNAL_DSS_H

#include <linux/bio.h>

int iotrace_dss_bio_io_class(struct bio *bio);

/**
 * @note These values are actually defined by other component
 */
enum {
    DSS_UNCLASSIFIED = 0,
    DSS_METADATA = 1,
    DSS_DATA_DIR = 7,
    DSS_DATA_FILE_4KB = 11,
    DSS_DATA_FILE_16KB = 12,
    DSS_DATA_FILE_64KB = 13,
    DSS_DATA_FILE_256KB = 14,
    DSS_DATA_FILE_1MB = 15,
    DSS_DATA_FILE_4MB = 16,
    DSS_DATA_FILE_16MB = 17,
    DSS_DATA_FILE_64MB = 18,
    DSS_DATA_FILE_256MB = 19,
    DSS_DATA_FILE_1GB = 20,
    DSS_DATA_FILE_BULK = 21,
    DSS_DATA_DIRECT = 22,
    DSS_MISC = 23,
    DSS_MAX = 33,
};

#endif  // SOURCE_KERNEL_INTERNAL_DSS_H

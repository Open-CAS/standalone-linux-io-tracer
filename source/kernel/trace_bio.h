/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef INTERNAL_TRACE_BIO_H_
#define INTERNAL_TRACE_BIO_H_

struct iotrace_context;
struct bio;

/**
 * @brief Write I/O information to trace buffer
 *
 * @param context IO trace context
 * @param cpu CPU id
 * @param dev_id Device id
 * @param bio IO
 */
void iotrace_trace_bio(struct iotrace_context *context,
                       unsigned cpu,
                       uint64_t dev_id,
                       const struct bio *bio);

#endif  // INTERNAL_TRACE_BIO_H_

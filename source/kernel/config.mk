# Copyright(c) 2012-2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear

override EXTRA_CFLAGS += -I$(M)
override EXTRA_CFLAGS += -I$(GENERATED_HEADER)

override EXTRA_CFLAGS += -Werror
override EXTRA_CFLAGS += -DIOTRACE_VERSION=$(IOTRACE_VERSION)
ifneq ($(IOTRACE_VERSION_LABEL),)
override EXTRA_CFLAGS += -DIOTRACE_VERSION_LABEL=$(IOTRACE_VERSION_LABEL)
endif

#!/bin/bash
#
## Copyright(c) 2012-2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
# #
#

obj-m += test_mod.o

MAKE_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(MAKE_DIR) modules EXTRA_CFLAGS='-Werror -Wno-error=unused-variable -Wno-error=uninitialized'
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(MAKE_DIR) clean

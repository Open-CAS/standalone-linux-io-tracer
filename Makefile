# Copyright(c) 2012-2018 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear

SHELL := /bin/bash
.DEFAULT_GOAL := all
CMAKE_FILE=CMakeLists.txt

ifdef DEBUG
	BUILD_DIR=build/debug
	ifndef PREFIX
		PREFIX=build/debug/rootfs
	endif
else
	BUILD_DIR=build/release
	ifndef PREFIX
		PREFIX=/
		RUN_LD_CONFIG=1
	endif
endif
SOURCE_PATH:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))


.PHONY: init all clean

init:
	mkdir -p $(BUILD_DIR)

all: init
	cd $(BUILD_DIR) && cmake $(SOURCE_PATH) -DCMAKE_INSTALL_PREFIX=$(PREFIX)
	$(MAKE) -C $(BUILD_DIR) all

install: all
	cmake -P $(BUILD_DIR)/cmake_install.cmake

uninstall:
	xargs rm -v -f < $(BUILD_DIR)/install_manifest.txt

test:

clean:
	$(info Cleaning $(BUILD_DIR))
	@if [ -d $(BUILD_DIR) ] ; \
	then \
		$(MAKE) -C $(BUILD_DIR) clean ; \
		$(MAKE) -C $(BUILD_DIR) clean-module ; \
		rm -rf $(BUILD_DIR) ; \
	fi

# Copyright(c) 2012-2018 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

SHELL := /bin/bash
.DEFAULT_GOAL := all
CMAKE_FILE=CMakeLists.txt

ifdef DEBUG
	ifndef BUILD_DIR
		BUILD_DIR=build/debug
	endif
	BUILD_TYPE=DEBUG
	ifndef PREFIX
		PREFIX=./rootfs
	endif
else
	ifndef BUILD_DIR
		BUILD_DIR=build/release
	endif
	BUILD_TYPE=RELEASE
	ifndef PREFIX
		PREFIX=/
	endif
endif

SOURCE_PATH:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
CMAKE:=$(SOURCE_PATH)/modules/open-cas-telemetry-framework/tools/third_party/cmake/bin/cmake
ifeq ("$(wildcard $(CMAKE))","")
   $(info Using system $(shell cmake --version | grep version))
   CMAKE=cmake
endif

.PHONY: init all clean

init:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(CMAKE) $(SOURCE_PATH) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_INSTALL_PREFIX=$(PREFIX)

all: init
	$(MAKE) -C $(BUILD_DIR) all

install: all
	$(MAKE) uninstall
	$(CMAKE) -DCOMPONENT=octf-install -P $(BUILD_DIR)/cmake_install.cmake
	$(CMAKE) -DCOMPONENT=octf-install-headers -P $(BUILD_DIR)/cmake_install.cmake
	$(CMAKE) -DCOMPONENT=octf-install-cmake -P $(BUILD_DIR)/cmake_install.cmake
	$(CMAKE) -DCOMPONENT=octf-post-install -P $(BUILD_DIR)/cmake_install.cmake
	$(CMAKE) -DCOMPONENT=iotrace-install -P $(BUILD_DIR)/cmake_install.cmake
	$(CMAKE) -DCOMPONENT=iotrace-post-install -P $(BUILD_DIR)/cmake_install.cmake

package: all
	$(MAKE) -C $(BUILD_DIR) package

package_source:
	$(MAKE) init
	$(CMAKE) -P version.cmake
	cd modules/open-cas-telemetry-framework && $(CMAKE) -P octf-version.cmake
	make -C $(BUILD_DIR) package_source

uninstall: init
	$(MAKE) -C $(BUILD_DIR) iotrace-uninstall
	$(MAKE) -C $(BUILD_DIR) octf-uninstall

uninstall-tracer: init
	cd $(BUILD_DIR) && $(CMAKE) $(SOURCE_PATH) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_INSTALL_PREFIX=$(PREFIX)
	$(MAKE) -C $(BUILD_DIR) iotrace-uninstall

test:

clean:
	$(info Cleaning $(BUILD_DIR))
	@if [ -d $(BUILD_DIR) ] ; \
	then \
		if [ -f $(BUILD_DIR)/Makefile ] ; \
		then \
			$(MAKE) -C $(BUILD_DIR) clean ; \
		fi ; \
		rm -rf $(BUILD_DIR) ; \
	fi

#!/bin/bash

# Copyright(c) 2012-2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear

IOTRACE_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
OCTF_DIR=${IOTRACE_DIR}/modules/open-cas-telemetry-framework

# Include common script utilites from OCTF
PREFIX="IOTRACE"
. ${OCTF_DIR}/tools/scripts/octf-common.sh

function iotrace_get_kernel_package () {
    case "${DISTRO}" in
    "RHEL7"|"RHEL8"|"CENTOS7"|"CENTOS8"|"FEDORA")
        echo "kernel-devel"
        ;;
    "UBUNTU")
        echo "linux-headers"
        ;;
    *)
        error "Unknown Linux distribution"
        ;;
    esac
}

function iotrace_get_distribution_pkg_dependencies () {
    case "${DISTRO}" in
    "RHEL7"|"CENTOS7"|"FEDORA")
        echo "rpm-build"
        ;;
    "RHEL8"|"CENTOS8")
        echo "rpm-build elfutils-libelf-devel"
        ;;
    "UBUNTU")
        echo "dpkg"
        ;;
    *)
        error "Unknown Linux distribution"
        ;;
    esac
}

function iotrace_setup_kernel_headers () {
    local kernel_version=$(uname -r)
    local kernel_dir=$(readlink -f /lib/modules/${kernel_version}/build)

    if [ -d "${kernel_dir}" ]
    then
        info "Kernel headers already available: ${kernel_dir}"
        return 0
    fi

    info "Install kernel headers"
    local installer=$(get_distribution_pkg_manager)
    ${installer} $(iotrace_get_kernel_package)

    if [ ! -d "${kernel_dir}" ]
    then
        info "Cannot install kernel headers appropriate to running kernel"
        info "Try to install specific kernel headers version: ${kernel_version}"

        case "${DISTRO}" in
        "RHEL7"|"CENTOS7"|"RHEL8"|"CENTOS8"|"FEDORA")
            ${installer} "kernel-devel-uname-r == $(uname -r)"
            ;;
        "UBUNTU")
            ${installer} linux-headers-$(uname -r)
            ;;
        *)
            error "Unknown Linux distribution"
            ;;
        esac
    fi

    if [ ! -d "${kernel_dir}" ]
    then
        info "Consider updating the system kernel to match available headers"
        error "The installed kernel headers do not match to the running kernel version."
    fi

    return 0
}

SCRIPTPATH=`dirname $0`
SCRIPTPATH=`realpath $SCRIPTPATH`
CONFIG_FILES=`ls $SCRIPTPATH/configure.d/*.conf | sort`
CONFIG_FILE=$SCRIPTPATH/"config.out"

function generate_config() {
	rm -f ${CONFIG_FILE}
	touch ${CONFIG_FILE}
	n_cores=$(nproc)

	# Compile each test module in background
	echo "Preparing configuration"
		for file in $CONFIG_FILES; do
			# $1 - Action to be performed
			# $2 - File with stored configuration
			# $3 - Name of called script (since script is running as subprocess
			#		it has to be passed explicitly)
			source $file "check" "$CONFIG_FILE" "$file" &

			# Prevent spawning more subprocesses than CPU available
			while [ $(ps --no-headers -o pid --ppid=$$ | wc -w) -ge $n_cores ] ; do
				sleep 1
			done
	done

	# Wait for all compilation processes to finish
	wait

	grep "X" ${CONFIG_FILE} &> /dev/null
	if [ $? -eq 0 ] ; then
		echo "ERROR! Following steps failed while preparing config:"
		grep "X" ${CONFIG_FILE} | cut -f1 -d ' '
		exit 1
	fi
}

function generate_header() {
	rm -f $SCRIPTPATH/source/kernel/generated_config.h
	# Configs starting with '1_' have to be put as first in header
	FIRST=$(echo $CONFIG_FILES | tr ' ' '\n' | grep '1_')
	SECOND=$(echo $CONFIG_FILES | tr ' ' '\n' | grep '2_')

	for file in $FIRST; do
		CONF=$(cat ${CONFIG_FILE} | grep $(basename $file) | cut -d' ' -f2)
		source $file "apply" "$CONF" "$file"
	done

	for file in $SECOND; do
		CONF=$(cat ${CONFIG_FILE} | grep $(basename $file) | cut -d' ' -f2)
		source $file "apply" "$CONF" "$file"
	done
}

if [ "$EUID" -ne 0 ]
then
    error "Please run as root to allow using package manager"
fi

# Include and execute OCTF setup dependencies
bash -c ${OCTF_DIR}/setup_dependencies.sh
if [ $? -ne 0 ]
then
    error "Cannot setup OCTF submodule dependencies"
    exit 1
fi

PKGS=$(iotrace_get_distribution_pkg_dependencies)
install_pacakges $PKGS
iotrace_setup_kernel_headers
generate_config
generate_header

exit 0

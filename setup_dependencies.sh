#!/bin/bash

# Copyright(c) 2012-2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

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
    "UBUNTU"|"DEBIAN")
        echo "linux-headers"
        ;;
    *)
        error "Unknown Linux distribution"
        ;;
    esac
}

function iotrace_get_distribution_pkg_dependencies () {
    local pkgs="zlib clang llvm libblkid-devel bpftool libbpf-devel"

    case "${DISTRO}" in
    "RHEL7"|"CENTOS7"|"FEDORA")
        echo "${pkgs} rpm-build elfutils-libelf-devel"
        ;;
    "RHEL8"|"CENTOS8")
        echo "${pkgs} rpm-build elfutils-libelf-devel"
        ;;
    "UBUNTU"|"DEBIAN")
        echo "${pkgs} dpkg"
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
        "UBUNTU"|"DEBIAN")
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

exit 0

#!/bin/bash

# Copyright(c) 2012-2018 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear

function detect_distribution ()
{
    if [ -f /etc/redhat-release ]
    then
        if ( cat /etc/redhat-release | grep "Red Hat Enterprise Linux Server release 7." &>/dev/null )
        then
            echo RHEL7
            return 0
        elif ( cat /etc/redhat-release | grep "Red Hat Enterprise Linux Server release 8." &>/dev/null )
        then
            echo RHEL8
            return 0
        fi
    fi

    if [ -f /etc/centos-release ]
    then
        if ( cat /etc/centos-release | grep "CentOS Linux release 7." &>/dev/null )
        then
            echo CENTOS7
            return 0
        elif ( cat /etc/centos-release | grep "CentOS Linux release 8." &>/dev/null )
        then
            echo CENTOS8
            return 0
        fi
    fi

    if [ -f /etc/fedora-release ]
    then
        if ( cat /etc/fedora-release | grep "Fedora release" &>/dev/null )
        then
            echo FEDORA
            return 0
        fi
    fi

    if [ -f /etc/os-release ]
    then
        if ( cat /etc/os-release | grep "Ubuntu" &>/dev/null )
        then
            echo UBUNTU
            return 0
        fi
    fi

    return 1
}

#
# Usage: iotrace_check_result <RESULT> <ERROR_MESSAGE>
#
function iotrace_check_result ()
{
    local result=$1
    local message=$2

    if [ ${result} -ne 0 ]
    then
        printf "[IOTRACE][ERROR] ${message}\n" 1>&2
        exit ${result}
    fi
}

#
# Usage: iotrace_error <ERROR_MESSAGE_1> [ <ERROR_MESSAGE_2> ... ]
# Note: exits with error
#
function iotrace_error () {
    iotrace_check_result 255 "$*"
}

#
# Usage: iotrace_info <INFO_MESSAGE_1> [ <INFO_MESSAGE_2> ... ]
#
function iotrace_info () {
    echo "[IOTRACE][INFO] $*" 1>&2
}

function iotrace_get_kernel_package () {
    distro=$(detect_distribution)
    case "${distro}" in
    "RHEL7"|"RHEL8"|"CENTOS7"|"CENTOS8"|"FEDORA")
        echo "kernel-devel"
        ;;
    "UBUNTU")
        echo "linux-headers"
        ;;
    *)
        iotrace_error "Unknown Linux distribution"
        ;;
    esac
}

function iotrace_get_distribution_pkg_dependencies () {
    distro=$(detect_distribution)
    case "${distro}" in
    "RHEL7"|"CENTOS7"|"FEDORA")
        echo "rpm-build"
        ;;
    "RHEL8"|"CENTOS8")
        echo "rpm-build elfutils-libelf-devel"
        ;;
    "UBUNTU")
        echo "rpm dpkg"
        ;;
    *)
        iotrace_error "Unknown Linux distribution"
        ;;
    esac
}

function iotrace_setup_kernel_headers () {
    local kernel_version=$(uname -r)
    local kernel_dir=$(readlink -f /lib/modules/${kernel_version}/build)

    if [ -d "${kernel_dir}" ]
    then
        iotrace_info "Kernel headers already available: ${kernel_dir}"
        return 0
    fi

    iotrace_info "Install kernel headers"
    ${PKG_INSTALLER} $(iotrace_get_kernel_package)
    iotrace_check_result $? "Cannot install kernel headers"

    kernel_dir=$(readlink -f /lib/modules/${kernel_version}/build)
    if [ ! -d "${kernel_dir}" ]
    then
        iotrace_info "Consider updating the system kernel to match available headers"
        iotrace_error "The installed kernel headers do not match to the running kernel version."
    fi

    return 0
}

function iotrace_get_distribution_pkg_manager () {
    distro=$(detect_distribution)
    case "${distro}" in
    "RHEL7"|"RHEL8"|"CENTOS7"|"CENTOS8"|"FEDORA")
        echo "yum -y install"
        ;;
    "UBUNTU")
        echo "apt-get -y install"
        ;;
    *)
        error "Unknown Linux distribution"
        ;;
    esac
}

# Include and execute OCTF setup dependencies
IOTRACE_SCRIPT_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
bash -c ${IOTRACE_SCRIPT_DIR}/modules/open-cas-telemetry-framework/setup_dependencies.sh
if [ $? -ne 0 ]
then
    iotrace_error "Cannot setup OCTF submodule dependencies"
    exit 1
fi

if [ "$EUID" -ne 0 ]
then
    iotrace_error "Please run as root to allow using package manager"
fi

PKGS=$(iotrace_get_distribution_pkg_dependencies)
PKG_INSTALLER=$(iotrace_get_distribution_pkg_manager)
iotrace_setup_kernel_headers
iotrace_info "Installing packages: ${PKGS}"
${PKG_INSTALLER} ${PKGS}
iotrace_check_result $? "Cannot install required dependencies"

exit 0

#!/bin/bash

# Copyright(c) 2012-2018 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear


#
# Usage: check_result <RESULT> <ERROR_MESSAGE>
#
function check_result ()
{
    local result=$1
    local message=$2

    if [ ${result} -ne 0 ]
    then
        echo "[OCTF][ERROR] ${message}" 1>&2
        exit ${result}
    fi
}

#
# Usage: error <ERROR_MESSAGE_1> [ <ERROR_MESSAGE_2> ... ]
# Note: exits with error
#
function error () {
    check_result 255 "$*"
}

#
# Usage: info <INFO_MESSAGE_1> [ <INFO_MESSAGE_2> ... ]
#
function info () {
    echo "[OCTF][INFO] $*" 1>&2
}

function detect_distribution ()
{
    if [ -f /etc/redhat-release ] || [ -f /etc/centos-release ]
    then
        if ( cat /etc/redhat-release | grep "Red Hat Enterprise Linux Server release 7." &>/dev/null )
        then
            echo RHEL7
            return 0
        fi
    fi

    if [ -f /etc/redhat-release ] || [ -f /etc/centos-release ]
    then
        if ( cat /etc/centos-release | grep "CentOS Linux release 7." &>/dev/null )
        then
            echo CENTOS7
            return 0
        fi
    fi

    if [ -f /etc/fedora-release ]
    then
        if ( cat /etc/fedora-release | grep "Fedora release 30" &>/dev/null )
        then
            echo FEDORA30
            return 0
        fi
    fi

    if [ -f /etc/os-release ]
    then
        if ( cat /etc/os-release | grep "Ubuntu 18" &>/dev/null )
        then
            echo UBUNTU18
            return 0
        fi
    fi

    return 1
}

function setup_other_deps
{
    if [ -f $(dirname "$0")/modules/open-cas-telemetry-framework/setup_dependencies.sh ]
    then
        $(dirname "$0")/modules/open-cas-telemetry-framework/setup_dependencies.sh
        check_result $? "Could not install dependencies"
    else
        info "No OCTF in the project source tree, assuming OCTF and dependencies are installed systemwide"
    fi
}


if [ "$EUID" -ne 0 ]
then
    echo "Please run as root to alllow using apt/yum and installing to /opt"
    exit 1
fi

distro=$(detect_distribution)
case "${distro}" in
"RHEL7")
    info "RHEL7.x detected"
    packages="kernel-headers-$(uname -r)"

    info "Installing packages: ${packages}"
    yum -y install ${packages}
    check_result $? "Cannot install required dependencies"

    if [ ! -d /usr/src/kernels/$(uname -r) ]
    then
        error "Linux kernel headers were not properly installed."
    fi

    setup_other_deps
    ;;
"CENTOS7")
    info "CentOS7.x detected"
    packages="kernel-headers-$(uname -r)"

    info "Installing packages: ${packages}"
    yum -y install ${packages}
    check_result $? "Cannot install required dependencies"

    if [ ! -d /usr/src/kernels/$(uname -r) ]
    then
        error "Linux kernel headers were not properly installed."
    fi

    setup_other_deps
    ;;
"FEDORA30")
    info "Fedora 30 detected"
    packages="kernel-devel-$(uname -r)"

    info "Installing packages: ${packages}"
    dnf -y install ${packages}
    check_result $? "Cannot install required dependencies"
    ;;
"UBUNTU18")
    info "Ubuntu 18 detected"
    packages="linux-headers-$(uname -r)"

    info "Installing packages: ${packages}"
    apt -y install ${packages}
    check_result $? "Cannot install required dependencies"

    if [ ! -d /usr/src/linux-headers-$(uname -r) ]
    then
        error "Linux kernel headers were not properly installed."
    fi

    setup_other_deps
    ;;
*)
    error "Unknown linux distribution"
    exit 1
    ;;
esac


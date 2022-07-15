#!/bin/bash

# Copyright(c) 2012-2020 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

SCRIPTPATH=`dirname $0`
SCRIPTPATH=`realpath $SCRIPTPATH`
CONFIG_FILES=`ls $SCRIPTPATH/configure.d/*.conf | sort`
CONFIG_FILE=$OUT/"config.out"

if [ "" == "${OUT}" ]
then
	echo "ERROR! Output directory not set, please set OUT varaible."
	exit 1
fi

function generate_config() {
	rm -f ${CONFIG_FILE}
	rm -Rf ${OUT}/configure.d
	touch ${CONFIG_FILE}
	n_cores=$(nproc)

	# Compile each test module in background
	echo "Preparing configuration"
		for file in $CONFIG_FILES; do
			# $1 - Action to be performed
			# $2 - File with stored configuration
			# $3 - Name of called script (since script is running as subprocess
			#		it has to be passed explicitly)
			source $file "check" "$CONFIG_FILE" "$file" KERNEL_DIR="$KERNEL_DIR"
	done

	grep "X" ${CONFIG_FILE} &> /dev/null
	if [ $? -eq 0 ] ; then
		echo "ERROR! Following steps failed while preparing config:"
		grep "X" ${CONFIG_FILE} | cut -f1 -d ' '
		exit 1
	fi
}

function generate_header() {
	rm -f $OUT/iotrace.bpf.config.h
	# Configs starting with '1_' have to be put as first in header
	FIRST=$(echo $CONFIG_FILES | tr ' ' '\n' | grep '1_')
	SECOND=$(echo $CONFIG_FILES | tr ' ' '\n' | grep '2_')

	for file in $FIRST; do
		CONF=$(cat ${CONFIG_FILE} | grep $(basename $file) | cut -d' ' -f2)
		source $file "apply" "$CONF" "$file" KERNEL_DIR="$KERNEL_DIR"
	done

	for file in $SECOND; do
		CONF=$(cat ${CONFIG_FILE} | grep $(basename $file) | cut -d' ' -f2)
		source $file "apply" "$CONF" "$file" KERNEL_DIR="$KERNEL_DIR"
	done
}

generate_config
generate_header

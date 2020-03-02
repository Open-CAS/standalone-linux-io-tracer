#!/bin/bash

# Check for required binaries
req_bins=(rsync tar git awk cmake)
for bin in "${req_bins[@]}"
do
	if ! [ -x "$(command -v ${bin})" ]; then
	  echo "Error: ${bin} is not installed." >&2
	  exit 1
	fi
done


script_dir="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
tmp_dir="${script_dir}/iotrace-src"
repo_dir=$(cd "${script_dir}/../../" && pwd)
repo_dir="${repo_dir}/./"

# Check if all submodules are checkout out
IFS=$'\n'
modules_paths=($(git submodule status --recursive | awk '{ print $2 }'))
for path in "${modules_paths[@]}"
do
	if [ ! -n "$(ls -A ${script_dir}/${path} 2>/dev/null)" ]
	then
		echo "${path} submodule is not checkout out!"
		exit 1
	fi
done

# Check for internal cmake
if [ ! -n "$(ls -A ${repo_dir}/modules/open-cas-telemetry-framework/tools/third_party/cmake 2>/dev/null)" ]
then
	echo "Third party cmake binaries not present!"
	exit 1
fi

# Generate VERSION file
cmake -P ${repo_dir}/version.cmake
if [ ! -f "${script_dir}/VERSION" ]; then
    echo "VERSION file was not generated!"
fi

mkdir -p "${tmp_dir}"
echo "Copying files..."
rsync -lRr --info=progress2 --exclude='build' --exclude='*/build' --exclude='iotrace-src' --exclude='*CMakeFiles*' "${repo_dir}" "${tmp_dir}/"

mv "${script_dir}/VERSION" "${tmp_dir}"

echo "Creating tar.gz archive..."
tar -czf iotrace-src.tar.gz --owner=0 --group=0 -C "${tmp_dir}/.." iotrace-src

echo "Removing tmp files..."
rm -rf ${tmp_dir}
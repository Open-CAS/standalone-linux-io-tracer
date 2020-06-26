# Built kernel module path
set(traceModulePath ${CMAKE_CURRENT_SOURCE_DIR}/iotrace.ko)
set(generatedHeader ${CMAKE_CURRENT_BINARY_DIR}/generated_config.h)

if(DEFINED ENV{KERNEL_SRCS})
    set(kernelSourcesPath $ENV{KERNEL_SRCS})
    message(STATUS "Custom kernel sources path chosen: $ENV{KERNEL_SRCS}")
else()
    execute_process(OUTPUT_VARIABLE kernelName OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND uname -r)
    set(kernelSourcesPath "/lib/modules/${kernelName}/build")
endif()

set(moduleSources
    "${CMAKE_CURRENT_LIST_DIR}/config.h"
    "${CMAKE_CURRENT_LIST_DIR}/context.h"
    "${CMAKE_CURRENT_LIST_DIR}/io_trace.h"
    "${CMAKE_CURRENT_LIST_DIR}/procfs.h"
    "${CMAKE_CURRENT_LIST_DIR}/trace_bdev.h"
    "${CMAKE_CURRENT_LIST_DIR}/trace_bio.h"
    "${CMAKE_CURRENT_LIST_DIR}/trace_env_kernel.h"
    "${CMAKE_CURRENT_LIST_DIR}/io_trace.c"
    "${CMAKE_CURRENT_LIST_DIR}/iotrace_event.h"
    "${CMAKE_CURRENT_LIST_DIR}/main.c"
    "${CMAKE_CURRENT_LIST_DIR}/procfs_files.h"
    "${CMAKE_CURRENT_LIST_DIR}/procfs.c"
    "${CMAKE_CURRENT_LIST_DIR}/trace_bdev.c"
    "${CMAKE_CURRENT_LIST_DIR}/trace_bio.c"
    "${CMAKE_CURRENT_LIST_DIR}/trace.c"
    "${CMAKE_CURRENT_LIST_DIR}/trace.h"
    "${CMAKE_CURRENT_LIST_DIR}/trace_inode.h"
    "${CMAKE_CURRENT_LIST_DIR}/trace_inode.c"
)

set(headerSources
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_access_ok.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_bdev_lookup.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_bio_attr.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_bio_discard.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_bio_flags.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_bio_trace_completion.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_fs_notify_add_mark.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_fs_notify_handle_event.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_kernel_read.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_trace_register.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_vm_fault.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_vm_ops.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_write_hint.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/2_bio_flush.conf"
)

add_custom_command(OUTPUT ${generatedHeader}
    COMMAND
    OUT=${CMAKE_CURRENT_BINARY_DIR}
    KERNEL_DIR=${kernelSourcesPath}
    ${CMAKE_CURRENT_LIST_DIR}/generate_header.sh
    DEPENDS ${headerSources} ${kernelSourcesPath}
)

# Command for building iotrace.ko kernel module
# Module is built in source tree, and then all build artifacts are copied
# to binary directory
add_custom_command(
    OUTPUT  ${traceModulePath}
    COMMAND cd ${kernelSourcesPath} && make
    IOTRACE_VERSION="${IOTRACE_VERSION}"
    IOTRACE_VERSION_LABEL="${IOTRACE_VERSION_LABEL}"
    M=${CMAKE_CURRENT_SOURCE_DIR} modules
    GENERATED_HEADER=${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${moduleSources} ${generatedHeader}
)

add_custom_target(traceModule
    ALL
    DEPENDS ${traceModulePath}
)

add_custom_target(clean-module
    COMMAND cd ${kernelSourcesPath} && make M=${CMAKE_CURRENT_SOURCE_DIR} clean
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Install kernel module and run depmod
install(FILES "${traceModulePath}"
	DESTINATION "lib/modules/${kernelName}/extra"
	COMPONENT iotrace-install
)

if (${CMAKE_INSTALL_PREFIX} STREQUAL "/")
    install(
        CODE "execute_process(COMMAND depmod)"
        COMPONENT iotrace-post-install
    )
endif()

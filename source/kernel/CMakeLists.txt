# Built kernel module path
set(traceModulePath ${CMAKE_CURRENT_SOURCE_DIR}/iotrace.ko)

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

# Command for building iotrace.ko kernel module
# Module is built in source tree, and then all build artifacts are copied
# to binary directory
add_custom_command(
    OUTPUT  ${traceModulePath}
    COMMAND cd ${kernelSourcesPath} && make
    IOTRACE_VERSION="${IOTRACE_VERSION}"
    IOTRACE_VERSION_LABEL="${IOTRACE_VERSION_LABEL}"
    M=${CMAKE_CURRENT_SOURCE_DIR} modules
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${moduleSources}
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

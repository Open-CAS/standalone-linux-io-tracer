# Built kernel module path
set(traceModulePath ${CMAKE_CURRENT_BINARY_DIR}/iotrace.ko)

if(DEFINED ENV{KERNEL_SRCS})
		set(kernelSourcesPath $ENV{KERNEL_SRCS}/build)
		message(STATUS "Custom kernel sources path chosen: $ENV{KERNEL_SRCS}")
else()
		execute_process(OUTPUT_VARIABLE kernelName OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND uname -r)
		set(kernelSourcesPath "/lib/modules/${kernelName}/build")
endif()

set(moduleSources
	"${CMAKE_CURRENT_LIST_DIR}/dss.c"
	"${CMAKE_CURRENT_LIST_DIR}/main.c"
	"${CMAKE_CURRENT_LIST_DIR}/procfs.c"
	"${CMAKE_CURRENT_LIST_DIR}/trace_bdev.c"
	"${CMAKE_CURRENT_LIST_DIR}/io_trace.c"
	"${CMAKE_CURRENT_LIST_DIR}/trace.c"
	"${CMAKE_CURRENT_LIST_DIR}/trace.h"
	"${CMAKE_CURRENT_LIST_DIR}/iotrace_event.h"
	"${CMAKE_CURRENT_LIST_DIR}/internal/bio.h"
	"${CMAKE_CURRENT_LIST_DIR}/internal/context.h"
	"${CMAKE_CURRENT_LIST_DIR}/internal/dss.h"
	"${CMAKE_CURRENT_LIST_DIR}/internal/procfs.h"
	"${CMAKE_CURRENT_LIST_DIR}/internal/trace_bdev.h"
	"${CMAKE_CURRENT_LIST_DIR}/internal/io_trace.h"
	"${CMAKE_CURRENT_LIST_DIR}/../includes/procfs_files.h"
)

# Command for building iotrace.ko kernel module
# Module is built in source tree, and then all build artifacts are copied
# to binary directory
add_custom_command(
	OUTPUT  ${traceModulePath}
	COMMAND cd ${kernelSourcesPath} && make
	I=${CMAKE_CURRENT_LIST_DIR}/../includes
	IOTRACE_VERSION="${IOTRACE_VERSION}"
	IOTRACE_VERSION_LABEL="${IOTRACE_VERSION_LABEL}"
	M=${CMAKE_CURRENT_SOURCE_DIR} modules
	&& mv "${CMAKE_CURRENT_LIST_DIR}/*.o ${CMAKE_CURRENT_BINARY_DIR}"
	&& mv "${CMAKE_CURRENT_LIST_DIR}/*.ko ${CMAKE_CURRENT_BINARY_DIR}"
	&& mv "${CMAKE_CURRENT_LIST_DIR}/*order ${CMAKE_CURRENT_BINARY_DIR}"
	&& mv "${CMAKE_CURRENT_LIST_DIR}/*symvers ${CMAKE_CURRENT_BINARY_DIR}"
	&& mv "${CMAKE_CURRENT_LIST_DIR}/*mod.c ${CMAKE_CURRENT_BINARY_DIR}"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS ${moduleSources}
)

# Kernel module build target, depends on EL7 kernel headers
# NOTE: Checking if EL7 headers are installed is complicated
# so right now we check only if the running kernel is EL7
# or if we have user-specified headers path in KERNEL_SRCS
# TODO (trybicki): Provide a better way of checking headers
if (DEFINED ENV{KERNEL_SRCS} OR
	kernelName MATCHES ".*\.el7\..*")

	add_custom_target(traceModule
		ALL
		DEPENDS ${traceModulePath}
	)

	# Install kernel module and run depmod
	install(FILES "${traceModulePath}"
		DESTINATION "/lib/modules/${kernelName}/extra"
		COMPONENT iotrace-install
	)
	install(CODE "execute_process(COMMAND depmod)"
		COMPONENT iotrace-install
	)

else()
	message(WARNING "Tracing module is built on EL7 kernel only - skipping target. \
	Environment variable 'KERNEL_SRCS' can be set to override this behaviour")
endif()

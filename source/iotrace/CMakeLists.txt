set(LIBBPF_INCLUDE_DIRS ${LIBBPF_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR})

find_package(Protobuf 3.0 REQUIRED)
find_package(BpfObject REQUIRED)

set(protoSources ${CMAKE_CURRENT_LIST_DIR}/proto/InterfaceKernelTraceCreating.proto)

add_executable(iotrace "")

target_include_directories(iotrace PRIVATE ${PROTOBUF_INCLUDE_DIRS})
target_include_directories(iotrace PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# Tell protoc where to look for imported files
list(APPEND PROTOBUF_IMPORT_DIRS "${OCTF_PROTOBUF_IMPORT_DIRS}")

# Run protoc - create c++ files from .proto definitions
protobuf_generate_cpp(generatedSrcs generatedHdrs ${protoSources})

# Create version definitions
target_compile_definitions(iotrace
    PUBLIC
    IOTRACE_VERSION=${IOTRACE_VERSION}
)

if (IOTRACE_VERSION_LABEL)
    target_compile_definitions(iotrace
        PUBLIC
        IOTRACE_VERSION_LABEL=${IOTRACE_VERSION_LABEL}
    )
endif()

set(generatedVmlinux ${CMAKE_CURRENT_BINARY_DIR}/vmlinux.h)
set(configHeader ${CMAKE_CURRENT_BINARY_DIR}/iotrace.bpf.config.h)

set(headerSources
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_bio_write_hint.conf"
    "${CMAKE_CURRENT_LIST_DIR}/configure.d/1_rq_write_hint.conf"
    "${CMAKE_CURRENT_LIST_DIR}/iotrace.bpf.defs.h"
    "${CMAKE_CURRENT_LIST_DIR}/iotrace.bpf.common.h"
)

add_custom_command(OUTPUT ${configHeader}
    COMMAND
    OUT=${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/configure.sh
    COMMAND touch ${CMAKE_CURRENT_LIST_DIR}/iotrace.bpf.c
    DEPENDS ${generatedVmlinux} ${headerSources}
)
add_custom_target(configHeader
    DEPENDS ${configHeader}
)

bpf_object(iotrace iotrace.bpf.c)
add_dependencies(iotrace_skel configHeader)

# Link to octf library
target_link_libraries(iotrace PRIVATE octf)
target_link_libraries(iotrace PRIVATE iotrace_skel)
target_link_libraries(iotrace PRIVATE blkid)
target_link_libraries(iotrace PRIVATE bpf)

target_sources(iotrace
PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/InterfaceKernelTraceCreatingImpl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/KernelRingTraceProducer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/KernelTraceExecutor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/main.cpp
        ${generatedSrcs}
        ${generatedHdrs}
)

install(TARGETS iotrace
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT iotrace-install
)

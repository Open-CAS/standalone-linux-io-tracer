find_package(Protobuf 3.0 REQUIRED)

add_executable(iotrace "")

# Specify include path for generated proto headers
target_include_directories(iotrace PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_sources(iotrace
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
)

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

set(LIBBPF_INCLUDE_DIRS ${LIBBPF_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR})
find_package(BpfObject REQUIRED)
bpf_object(iotrace iotrace.bpf.c)

# Link to octf library
target_link_libraries(iotrace PRIVATE octf)
target_link_libraries(iotrace PRIVATE iotrace_skel)
target_link_libraries(iotrace PRIVATE blkid)

install(TARGETS iotrace
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT iotrace-install
)

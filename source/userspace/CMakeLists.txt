include(FindProtobuf)
find_package(Protobuf REQUIRED)

set(protoSources ${CMAKE_CURRENT_LIST_DIR}/proto/InterfaceKernelTraceCreating.proto)

add_executable(iotrace "")

target_include_directories(iotrace PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../includes")
target_include_directories(iotrace PRIVATE ${PROTOBUF_INCLUDE_DIRS})

# Specify include path for generated proto headers
target_include_directories(iotrace PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# Tell protoc where to look for imported files
list(APPEND PROTOBUF_IMPORT_DIRS "${OCTF_PROTOBUF_IMPORT_DIRS}")

# Run protoc - create c++ files from .proto definitions
protobuf_generate_cpp(generatedSrcs generatedHdrs ${protoSources})

target_sources(iotrace
PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/InterfaceKernelTraceCreatingImpl.cpp
		${CMAKE_CURRENT_LIST_DIR}/KernelRingTraceProducer.cpp
		${CMAKE_CURRENT_LIST_DIR}/KernelTraceExecutor.cpp
		${CMAKE_CURRENT_LIST_DIR}/main.cpp
		${generatedSrcs}
		${generatedHdrs}
)

# Create version definitions
target_compile_definitions(iotrace
    PUBLIC
    IOTRACE_VERSION="${IOTRACE_VERSION}"
    IOTRACE_VERSION_LABEL="${IOTRACE_VERSION_LABEL}"
)

install(TARGETS iotrace
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		COMPONENT iotrace-install
)

# Link to octf library
target_link_libraries(iotrace PRIVATE octf)

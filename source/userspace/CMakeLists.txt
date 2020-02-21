set(CMAKE_PREFIX_PATH "/opt/octf/protobuf;${CMAKE_PREFIX_PATH}")
find_package(Protobuf 3.0 REQUIRED)

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
    IOTRACE_VERSION=${IOTRACE_VERSION}
)

if (IOTRACE_VERSION_LABEL)
    target_compile_definitions(iotrace
        PUBLIC
        IOTRACE_VERSION_LABEL=${IOTRACE_VERSION_LABEL}
    )
endif()

# Link to octf library
target_link_libraries(iotrace PRIVATE octf)

install(TARGETS iotrace
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		COMPONENT iotrace-install
)
install(FILES ${PROJECT_BINARY_DIR}/VERSION
	DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR}
	COMPONENT iotrace-install
)


# Install install-manifest with list of installed files, to allow uninstalling
# Append install-manifest to itself
set(IOTRACE_MANIFEST_PREINSTALL ${PROJECT_BINARY_DIR}/install_manifest_iotrace-install.txt)
set(IOTRACE_MANIFEST_POSTINSTALL ${CMAKE_INSTALL_FULL_DOCDIR}/install_manifest_iotrace-install.txt)

install(CODE
	"file(APPEND ${IOTRACE_MANIFEST_PREINSTALL} \"\n${IOTRACE_MANIFEST_POSTINSTALL}\")"
	COMPONENT iotrace-post-install
)
install(FILES ${IOTRACE_MANIFEST_PREINSTALL}
	DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR}
	COMPONENT iotrace-post-install
)

# Add a target for uninstallation.
add_custom_target(iotrace-uninstall
	COMMAND test -f ${IOTRACE_MANIFEST_POSTINSTALL} &&
	 xargs rm -vf < ${IOTRACE_MANIFEST_POSTINSTALL} ||
	 echo "-- No iotrace install manifest found: ${IOTRACE_MANIFEST_POSTINSTALL} Nothing to uninstall!"
)


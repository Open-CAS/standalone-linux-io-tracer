if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/open-cas-telemetry-framework/CMakeLists.txt)
    add_subdirectory(open-cas-telemetry-framework)
endif()

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_LIST_DIR}/libbpf-bootstrap/tools/cmake
    CACHE STRING "Modules for CMake" FORCE
)

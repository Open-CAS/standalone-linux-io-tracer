cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

include(version.cmake)
project(iotrace VERSION ${IOTRACE_VERSION} LANGUAGES C CXX)

include (GNUInstallDirs)
include(defenses.cmake)
include(install.cmake)

add_subdirectory(modules)

if (NOT TARGET octf)
	# OCTF is not a part of this buildsystem, import it as a package
	find_package(octf REQUIRED CONFIG)
endif()

# Rpath for local installation of OCTF
if (NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/")
    set(CMAKE_INSTALL_RPATH "${OCTF_LIBRARY_DIR}")
endif()

add_subdirectory(source)

include(package.cmake)


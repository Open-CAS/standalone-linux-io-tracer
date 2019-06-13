cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 11)

include(version.cmake)
project(iotrace VERSION ${IOTRACE_VERSION} LANGUAGES C CXX)

include (GNUInstallDirs)
include(defenses.cmake)

add_subdirectory(modules)

if (NOT TARGET octf)
	# OCTF is not a part of this buildsystem, import it as a package
	find_package(octf REQUIRED CONFIG)
endif()

add_subdirectory(source)
add_subdirectory(doc)

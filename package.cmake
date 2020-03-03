find_program (hasYum yum)
find_program (hasDnf dnf)
find_program (hasApt apt)

if (hasYum OR hasDnf)
    set(CPACK_RPM_COMPONENT_INSTALL ON)
    list(APPEND CPACK_GENERATOR "RPM")
else()
    set(CPACK_RPM_COMPONENT_INSTALL OFF)
endif()

if (hasApt)
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    list(APPEND CPACK_GENERATOR "DEB")
else()
    set(CPACK_DEB_COMPONENT_INSTALL OFF)
endif()
execute_process(OUTPUT_VARIABLE uname_r OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND uname -r)


# Separate install and post-install components need to be specified because
# install(CODE) and install(SCRIPT) code is run at "make install" time. By
# default, CPack does a "make install" to an intermediate location in the build
# tree (underneath _CPack_Packages in your build tree) as part of building the
# final installer. We dont't want 'make install' code (e.g. depmod) to be run
# during 'make package' We add such code as a post install script.

# Set components to be installed with package
set(CPACK_COMPONENTS_ALL iotrace-install octf-install)

# Generate postinst/prerm scripts with build-time kernel version variable added
set(destPostinst ${CMAKE_CURRENT_BINARY_DIR}/postinst)
file(WRITE ${destPostinst} "uname_r=\"${uname_r}\"\n")
file(READ ${CMAKE_SOURCE_DIR}/tools/installer/postinst postinstContent)
file(APPEND ${destPostinst} "${postinstContent}")

set(destPrerm ${CMAKE_CURRENT_BINARY_DIR}/prerm)
file(WRITE ${destPrerm} "uname_r=\"${uname_r}\"\n")
file(READ ${CMAKE_SOURCE_DIR}/tools/installer/prerm prermContent)
file(APPEND ${destPrerm} "${prermContent}")

set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE ${destPostinst})
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE ${destPrerm})
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/tools/installer/postrm)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA};${destPostinst}")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA};${destPrerm}")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA};${CMAKE_CURRENT_SOURCE_DIR}/tools/installer/postrm")

# All components are to be installed with one rpm
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Standalone Linux I/O tracer for kernel ${uname_r}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# CPack wrongly assumes we want to create these directories as our own, beause we copy files there
# which causes installation/uninstallation errors. This fixes it.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
	"/;/usr/local;/usr/local/include;/usr/local/lib;/run;/var;/var/lib;/lib;/lib/modules;/lib/modules/${uname_r};/lib/modules/${uname_r}/extra")
set(CPACK_PACKAGE_VERSION "${IOTRACE_VERSION}")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_VENDOR "Intel Corporation")
set(CPACK_PACKAGE_CONTACT "https://github.com/Open-CAS")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_FILE_NAME
 "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)

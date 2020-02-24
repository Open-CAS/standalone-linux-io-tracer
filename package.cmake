# Build rpm/deb package using CPack included with CMake
set(VERSION "20.3")
set(rpmName "iotrace")

# RPM and DEB only
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)


# Separate install and post-install components need to be specified because
# install(CODE) and install(SCRIPT) code is run at "make install" time. By
# default, CPack does a "make install" to an intermediate location in the build
# tree (underneath _CPack_Packages in your build tree) as part of building the
# final installer. We dont't want 'make install' code (e.g. depmod) to be run
# during 'make package' We add such code as a post install script.

# Set components to be installed with package
set(CPACK_COMPONENTS_ALL iotrace-install octf-install)

set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/tools/installer/postinst)
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/tools/installer/prerm)
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/tools/installer/postrm)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA};${CMAKE_CURRENT_SOURCE_DIR/tools/installer}postinst")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA};${CMAKE_CURRENT_SOURCE_DIR/tools/installer}prerm")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA};${CMAKE_CURRENT_SOURCE_DIR/tools/installer}postrm")

# All components are to be installed with one rpm
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)

execute_process(OUTPUT_VARIABLE UNAME_R OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND uname -r)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Standalone Linux I/O tracer for kernel ${UNAME_R}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# CPack wrongly assumes we want to create these directories as our own, beause we copy files there
# which causes installation/uninstallation errors. This fixes it.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
	"/;/usr/local;/usr/local/include;/usr/local/lib;/run;/var;/var/lib;/lib;/lib/modules;/lib/modules/${UNAME_R};/lib/modules/${UNAME_R}/extra")
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_PACKAGE_NAME ${rpmName})
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_VENDOR "Intel Corporation")
set(CPACK_PACKAGE_CONTACT "https://github.com/Open-CAS")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_FILE_NAME
 "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "RPM;DEB")

include(CPack)

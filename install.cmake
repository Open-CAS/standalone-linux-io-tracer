include(GNUInstallDirs)

# Due to a bug in cmake which chooses wrong project name for subprojects
# in DOC_DIR path, we manually specify project name in doc path
get_filename_component(IOTRACE_DOC_DIR "${CMAKE_INSTALL_FULL_DOCDIR}"  DIRECTORY)
set(IOTRACE_DOC_DIR "${IOTRACE_DOC_DIR}/${PROJECT_NAME}")

install(FILES ${PROJECT_BINARY_DIR}/VERSION
    DESTINATION ${IOTRACE_DOC_DIR}
    COMPONENT iotrace-install
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION ${IOTRACE_DOC_DIR}
    COMPONENT iotrace-install
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/NOTICE
    DESTINATION ${IOTRACE_DOC_DIR}
    COMPONENT iotrace-install
)

# Install install-manifest with list of installed files, to allow uninstalling
# Append install-manifest to itself
set(IOTRACE_MANIFEST_PREINSTALL ${CMAKE_BINARY_DIR}/install_manifest_iotrace-install.txt)
set(IOTRACE_MANIFEST_POSTINSTALL ${IOTRACE_DOC_DIR}/install_manifest_iotrace-install.txt)

install(CODE
    "file(APPEND ${IOTRACE_MANIFEST_PREINSTALL} \"\n${IOTRACE_MANIFEST_POSTINSTALL}\")"
    COMPONENT iotrace-post-install
)
install(FILES ${IOTRACE_MANIFEST_PREINSTALL}
    DESTINATION ${IOTRACE_DOC_DIR}
    COMPONENT iotrace-post-install
)

# Add a target for uninstallation.
add_custom_target(iotrace-uninstall
    COMMAND test -f ${IOTRACE_MANIFEST_POSTINSTALL} &&
     xargs rm -vf < ${IOTRACE_MANIFEST_POSTINSTALL} ||
     echo "-- No iotrace install manifest found: ${IOTRACE_MANIFEST_POSTINSTALL} Nothing to uninstall!"
)


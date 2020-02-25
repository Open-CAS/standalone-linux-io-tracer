# CMAKE_INSTALL_DATAROOTDIR (from GNUInstallDirs):
# read-only architecture-independent data root (share)
set(docDir "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_NAME}")

add_custom_target(man ALL)

add_custom_command(
	TARGET man
	COMMAND gzip -c ${CMAKE_CURRENT_SOURCE_DIR}/iotrace.8 > ${CMAKE_CURRENT_BINARY_DIR}/iotrace.8.gz
	OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/iotrace.8.gz
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/iotrace.8
)

install(
	FILES ${CMAKE_CURRENT_SOURCE_DIR}/iotrace.8.gz
	DESTINATION ${CMAKE_INSTALL_MANDIR}/man8
	COMPONENT iotrace-install-man
)

# Run mandb and hide output when VERBOSE is unspecified
install(
	CODE "execute_process(COMMAND bash -c \"
	if [ -z $VERBOSE ]; then
		mandb >/dev/null 2>/dev/null
	else
		mandb
	fi
	\")"
	COMPONENT iotrace-install-man
)

# Install LICENSE
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
	DESTINATION "${docDir}"
	COMPONENT iotrace-install
)

# Install VERSION
install(FILES "${PROJECT_BINARY_DIR}/VERSION"
	DESTINATION "${docDir}"
	COMPONENT iotrace-install
)

# Install NOTICE
install(FILES "${PROJECT_SOURCE_DIR}/NOTICE"
	DESTINATION "${docDir}"
	COMPONENT iotrace-install
)
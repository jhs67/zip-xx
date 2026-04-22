
# cache variables
set(PKGXX_RELEASE_SOURCE "" CACHE STRING "source location for tool release")
set(PKGXX_TOOL_RELEASE "" CACHE STRING "last install version of the tools")

if(NOT DEFINED ENV{VCPKG_ROOT})
	# set the tool download source
	set(TOOL_DOWNLOAD_URL "https://github.com/microsoft/vcpkg-tool/releases/download")

	# set the install location
	set(PKGXX_TOOL_INSTALL_DIR "${CMAKE_BINARY_DIR}/pkgxx_installed")
	set(INSTALL_DIR "${PKGXX_TOOL_INSTALL_DIR}/vcpkg")
	file(MAKE_DIRECTORY "${PKGXX_TOOL_INSTALL_DIR}")

	# try to read the tool release from the vcpkg.json file
	file(READ ${CMAKE_SOURCE_DIR}/vcpkg.json VCPKG_JSON)
	string(JSON TOOL_RELEASE_TAG ERROR_VARIABLE JSON_ERROR GET ${VCPKG_JSON} "$pkgxx" tool_release)
	if(JSON_ERROR)
		SET(TOOL_RELEASE_TAG "")
	endif()

	# try to get the tool release from the default registry
	if("${TOOL_RELEASE_TAG}" STREQUAL "")
		if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg-configuration.json")
			# load the default repository and baseline
			file(READ "${CMAKE_SOURCE_DIR}/vcpkg-configuration.json" VCPKG_CONFIGURATION_JSON)
			string(JSON REPOSITORY ERROR_VARIABLE JSON_ERROR GET ${VCPKG_CONFIGURATION_JSON} "default-registry" repository)
			if(JSON_ERROR)
				SET(REPOSITORY "")
			endif()
			string(JSON BASELINE ERROR_VARIABLE JSON_ERROR GET ${VCPKG_CONFIGURATION_JSON} "default-registry" baseline)
			if(JSON_ERROR)
				SET(BASELINE "")
			endif()

			if(NOT "${REPOSITORY}" STREQUAL "" AND NOT "${BASELINE}" STREQUAL "")
				# check if we've already downloaded the correct file
				set(RELEASE_SOURCE "${REPOSITORY}:${BASELINE}")
				if ("${RELEASE_SOURCE}" STREQUAL "${PKGXX_RELEASE_SOURCE}")
					set(TOOL_RELEASE_TAG "${PKGXX_TOOL_RELEASE}")
				endif()

				# no existing value
				if("${TOOL_RELEASE_TAG}" STREQUAL "")
					# just assume I can download a file using the github schema
					file(DOWNLOAD "${REPOSITORY}/raw/${BASELINE}/scripts/vcpkg-tool-metadata.txt"
						"${PKGXX_TOOL_INSTALL_DIR}/vcpkg-tool-metadata.txt" STATUS DOWNLOAD_STATUS)
					list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
					if (STATUS_CODE EQUAL 0)
						file(READ "${PKGXX_TOOL_INSTALL_DIR}/vcpkg-tool-metadata.txt" METADATA_CONTENT)
						string(REGEX MATCH "VCPKG_TOOL_RELEASE_TAG=([^\n]*)" LINE_MATCH "${METADATA_CONTENT}")
						if (LINE_MATCH)
							set(TOOL_RELEASE_TAG ${CMAKE_MATCH_1})
							set(PKGXX_RELEASE_SOURCE "${RELEASE_SOURCE}" CACHE STRING "" FORCE)
						endif()
					endif()
				endif()
			endif()
		endif()
	endif()

	if (TOOL_RELEASE_TAG STREQUAL "")
		message(FATAL_ERROR "Failed to get tool release version")
	endif()

	# check the executable
	set(VCPKG_EXECUTABLE "${INSTALL_DIR}/vcpkg${CMAKE_HOST_EXECUTABLE_SUFFIX}")

	# check the correct version is correctly installed
	if(NOT "${TOOL_RELEASE_TAG}" STREQUAL "${PKGXX_TOOL_RELEASE}" OR NOT EXISTS "${VCPKG_EXECUTABLE}")
		message("## pkgxx: install vcpkg tools version ${TOOL_RELEASE_TAG}")
		message("## pkgxx: installation target: ${INSTALL_DIR}")

		# clean up any invalid install
		file(REMOVE_RECURSE "${INSTALL_DIR}")
		file(MAKE_DIRECTORY "${INSTALL_DIR}")

		# figure the correct executable for the host
		set(EXECUTABLE_SOURCE vcpkg-glibc)
		if (CMAKE_HOST_WIN32)
			set(EXECUTABLE_SOURCE vcpkg.exe)
		elseif((CMAKE_HOST_APPLE))
			set(EXECUTABLE_SOURCE vcpkg-macos)
		endif()

		# download the executable
		file(DOWNLOAD "${TOOL_DOWNLOAD_URL}/${TOOL_RELEASE_TAG}/${EXECUTABLE_SOURCE}" "${VCPKG_EXECUTABLE}")

		# flag the file as executable
		if (NOT CMAKE_HOST_WIN32)
			execute_process(
				COMMAND chmod a+x "${VCPKG_EXECUTABLE}"
				RESULT_VARIABLE CHMOD_RESULT
			)
			if (NOT CHMOD_RESULT EQUAL 0)
				message(FATAL_ERROR "failed to set executable flag")
			endif()
		endif()

		# bootstrap the standalone environment
		set(ENV{VCPKG_ROOT} "${INSTALL_DIR}")
		execute_process(
			COMMAND ${VCPKG_EXECUTABLE} bootstrap-standalone
			WORKING_DIRECTORY "${INSTALL_DIR}"
			RESULT_VARIABLE BOOTSTRAP_RETURN
		)
		if (NOT BOOTSTRAP_RETURN EQUAL 0)
			message(FATAL_ERROR "failed to extract bootstrap archive")
		endif()

		# set the release tag
		set(PKGXX_TOOL_RELEASE "${TOOL_RELEASE_TAG}" CACHE STRING "" FORCE)
	endif()

	# set the vcpkg root
	set(ENV{VCPKG_ROOT} "${INSTALL_DIR}")
endif()

# Set the toolchain file
set(TARGET_TOOLCHAIN "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

# Chain load the existing toolchain
if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_TOOLCHAIN_FILE STREQUAL TARGET_TOOLCHAIN)
	# don't chainload a toolchain we previously set
	if(NOT PKGXX_SET_TOOLCHAIN STREQUAL CMAKE_TOOLCHAIN_FILE)
		set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE} CACHE STRING "")
	endif()
endif()

if(NOT CMAKE_TOOLCHAIN_FILE STREQUAL TARGET_TOOLCHAIN)
	set(CMAKE_TOOLCHAIN_FILE ${TARGET_TOOLCHAIN} CACHE STRING "" FORCE)
	set(PKGXX_SET_TOOLCHAIN ${TARGET_TOOLCHAIN} CACHE STRING "" FORCE)

	# Total hack, but if we don't change this, then you need to
	# clean your CMakeCache.txt after changing your vcpkg install
	set(Z_VCPKG_ROOT_DIR $ENV{VCPKG_ROOT} CACHE INTERNAL "" FORCE)
endif()

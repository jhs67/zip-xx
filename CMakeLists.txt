#
# zip-xx
#
cmake_minimum_required(VERSION 3.21)

# use pkgxx for dependencies
set(VCPKG_INSTALL_OPTIONS --no-print-usage)
include("cmake/pkgxx.cmake" OPTIONAL)

# keep the code pretty
include("cmake/clang-format.cmake")

# project definition
project(zip-xx VERSION 0.1.0)

# Options
option(ZIP_XX_BuildTests "Build the unit tests when BUILD_TESTING is enabled." ON)
option(ZIP_XX_WarnAsError "Build the code and tests with warnings as errors." ON)

# dependencies
find_package(ZLIB REQUIRED)

# Configuration
set(ZIP_XX_TARGET_NAME ${PROJECT_NAME})

# install
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# files
set(zip_xx_source
	source/zip_streambuf.cpp
	source/zip_ostream.cpp
	source/detail/deflate.cpp
	source/detail/constants.h
	source/detail/crc.h
	source/detail/deflate.h
	source/detail/endian.h
	source/detail/entry_record.h
	source/detail/level_map.h
	source/detail/records.h
	source/detail/timestamps.h
	source/detail/records.cpp
)

set(zip_xx_headers
	include/zip-xx/zip-xx.h
)

# Target
add_library(${ZIP_XX_TARGET_NAME} ${zip_xx_source} ${zip_xx_headers})
target_compile_features(${ZIP_XX_TARGET_NAME} PUBLIC cxx_std_20)

target_include_directories(
	${ZIP_XX_TARGET_NAME}
	PUBLIC
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${ZIP_XX_TARGET_NAME} PRIVATE ZLIB::ZLIB)

target_clang_format(${ZIP_XX_TARGET_NAME})

if(ZIP_XX_WarnAsError)
	target_compile_options(${ZIP_XX_TARGET_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Werror>
	)
endif()

# Tests

include(CTest)

if(BUILD_TESTING AND ZIP_XX_BuildTests)
	add_subdirectory(test)
endif()

# Install

install(TARGETS ${ZIP_XX_TARGET_NAME}
	EXPORT zip-xx-export
	INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(FILES ${zip_xx_headers} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/zip-xx")

# create a version file
write_basic_package_version_file(
	zip-xx-config-version.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)

# create config file
set(config_install_dir "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")
set(PATH_EXPORT_TARGETS "${config_install_dir}/zip-xx-targets.cmake")
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/zip-xx-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/zip-xx-config.cmake"
    PATH_VARS PATH_EXPORT_TARGETS
    INSTALL_DESTINATION "${config_install_dir}"
)

# install the cmake files
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/zip-xx-config-version.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/zip-xx-config.cmake"
	DESTINATION "${config_install_dir}"
)

# export the targets
install(EXPORT zip-xx-export
	FILE zip-xx-targets.cmake
	NAMESPACE zip-xx::
	DESTINATION "${config_install_dir}"
)

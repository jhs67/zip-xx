#
# Copyright(c) 2018 Jon Spencer.
# ISC License.
#

project(zip-xx-test)

find_package(Catch2 CONFIG REQUIRED)

enable_testing()

set(ZIP_XX_TEST_SRC
	unit-ostream.cpp
)

add_executable(${PROJECT_NAME} ${ZIP_XX_TEST_SRC})
target_link_libraries(${PROJECT_NAME} Catch2::Catch2WithMain)
target_link_libraries(${PROJECT_NAME} ${ZIP_XX_TARGET_NAME})
target_clang_format(${PROJECT_NAME})

if(ZIP_XX_WarnAsError AND NOT MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/W4>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Werror>
	)
endif()

add_test(
	NAME ${PROJECT_NAME}
	COMMAND ${PROJECT_NAME}
	WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)

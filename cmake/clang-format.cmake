
find_program(clang_format_executable clang-format)

function(add_clang_format target)
	if (clang_format_executable STREQUAL "clang_format_executable-NOTFOUND")
		return()
	endif()

	unset(sources)
	foreach(source ${ARGN})
		get_filename_component(source ${source} ABSOLUTE)
	    list(APPEND sources ${source})
	endforeach()

	set(target_name "${target}_format")

	add_custom_target(${target_name}
		COMMAND ${clang_format_executable} -i ${sources}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Formatting ${target}..."
	)

	if(TARGET format)
		add_dependencies(format ${target_name})
	else()
		add_custom_target(format DEPENDS ${target_name})
	endif()
endfunction()

function(target_clang_format target)
	get_target_property(target_sources ${target} SOURCES)
	add_clang_format(${target} ${target_sources})
endfunction()

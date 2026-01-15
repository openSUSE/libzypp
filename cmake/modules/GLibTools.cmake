# Taken from https://github.com/GNOME/evolution
#
# GLibTools.cmake
#
# Provides functions to run glib tools.
#
# Functions:
#
# glib_mkenums(_output_filename_noext _header_templ _src_tmpl _enums_header ...)
#    runs glib-mkenums to generate enumtypes .h and .c files from multiple
#    _enums_header using _header_templ and _src_tmpl template files. It searches for files in the current source directory and
#    exports to the current binary directory.
#
#    An example call is:
#        glib_mkenums(camel-enumtypes camel-enums.h CAMEL_ENUMTYPES_H)
#        which uses camel-enums.h as the source of known enums and generates
#        camel-enumtypes.h which will use the CAMEL_ENUMTYPES_H define
#        and also generates camel-enumtypes.c with the needed code.
#
# glib_genmarshal(_output_filename_noext _prefix _marshallist_filename)
#    runs glib-genmarshal to process ${_marshallist_filename} to ${_output_filename_noext}.c
#    and ${_output_filename_noext}.h files in the current binary directory, using
#    the ${_prefix} as the function prefix.
#
# glib_compile_resources _sourcedir _outputprefix _cname _inxml ...deps)
#    Calls glib-compile-resources as defined in _inxml and using _outputprefix and_cname as other arguments
#    beside _sourcedir. The optional arguments are other dependencies.

find_program(GLIB_MKENUMS glib-mkenums)
if(NOT GLIB_MKENUMS)
        message(FATAL_ERROR "Cannot find glib-mkenums, which is required to build ${PROJECT_NAME}")
endif(NOT GLIB_MKENUMS)

function( glib_mkenums _output_filename_noext _header_templ _src_tmpl _enums_header0 )

        foreach(_enums_header ${_enums_header0} ${ARGN})
                list(APPEND _enums_headers "${CMAKE_CURRENT_SOURCE_DIR}/${_enums_header}")
        endforeach(_enums_header)

        message("Generating file ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h")

        add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h
                COMMAND ${GLIB_MKENUMS} --template "${CMAKE_CURRENT_SOURCE_DIR}/${_header_templ}" ${_enums_headers} >${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_header_templ}" ${_enums_headers}
        )

        message("Generating file ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c")

        add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c
                COMMAND ${GLIB_MKENUMS} --template "${CMAKE_CURRENT_SOURCE_DIR}/${_src_tmpl}" ${_enums_headers} >${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_src_tmpl}" ${_enums_headers}
        )
endfunction( glib_mkenums )

find_program(GLIB_GENMARSHAL glib-genmarshal)
if(NOT GLIB_GENMARSHAL)
        message(FATAL_ERROR "Cannot find glib-genmarshal, which is required to build ${PROJECT_NAME}")
endif(NOT GLIB_GENMARSHAL)

function(glib_genmarshal _output_filename_noext _prefix _marshallist_filename)
        add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h
                COMMAND ${GLIB_GENMARSHAL} --header --skip-source --prefix=${_prefix} "${CMAKE_CURRENT_SOURCE_DIR}/${_marshallist_filename}" >${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h.tmp
                COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h.tmp ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h
                DEPENDS ${_marshallist_filename}
        )

        add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c
                COMMAND ${CMAKE_COMMAND} -E echo " #include \\\"${_output_filename_noext}.h\\\"" >${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c.tmp
                COMMAND ${GLIB_GENMARSHAL} --body --skip-source --prefix=${_prefix} "${CMAKE_CURRENT_SOURCE_DIR}/${_marshallist_filename}" >>${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c.tmp
                COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c.tmp ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c
                DEPENDS ${_marshallist_filename}
        )
endfunction(glib_genmarshal)

find_program(GLIB_COMPILE_RESOURCES glib-compile-resources)
if(NOT GLIB_COMPILE_RESOURCES)
        message(FATAL_ERROR "Cannot find glib-compile-resources, which is required to build ${PROJECT_NAME}")
endif(NOT GLIB_COMPILE_RESOURCES)

macro(glib_compile_resources _sourcedir _outputprefix _cname _inxml)
        add_custom_command(
                OUTPUT ${_outputprefix}.h
                COMMAND ${GLIB_COMPILE_RESOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/${_inxml} --target=${_outputprefix}.h --sourcedir=${_sourcedir} --c-name ${_cname} --generate-header
                DEPENDS ${_inxml} ${ARGN}
                VERBATIM
        )
        add_custom_command(
                OUTPUT ${_outputprefix}.c
                COMMAND ${GLIB_COMPILE_RESOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/${_inxml} --target=${_outputprefix}.c --sourcedir=${_sourcedir} --c-name ${_cname} --generate-source
                DEPENDS ${_inxml} ${ARGN}
                VERBATIM
        )
endmacro(glib_compile_resources)

include(UsePkgConfig)
include(MacroAddFileDependencies)
macro(glib_gen_marchal _sources _lists _prefix)
    foreach (_i ${_lists})
        get_filename_component(_marchal_list ${_i} ABSOLUTE)
        string(REGEX REPLACE "\\.list$" ".c" _body_name ${_i})
        set(_output_body ${CMAKE_CURRENT_SOURCE_DIR}/${_body_name})
        string(REGEX REPLACE "\\.list$" ".h" _header_name ${_i})
        set(_output_header ${CMAKE_CURRENT_SOURCE_DIR}/${_header_name})

        # Body
#         message(STATUS "GLib Marchal body: " ${_output_body}})
        add_custom_command(
            OUTPUT ${_output_body}
            COMMAND echo "\\#include \\\"${_header_name}\\\"" > ${_output_body}
            COMMAND glib-genmarshal ${_marchal_list} --prefix=${_prefix} --body >> ${_output_body}
        )

        # Header
#         message(STATUS "GLib Marchal hearder: " ${_output_header})
        add_custom_command(
            OUTPUT ${_output_header}
            COMMAND glib-genmarshal ${_marchal_list} --prefix=${_prefix} --header > ${_output_header}
        )

        SET(${_sources} ${${_sources}} ${_output_body} ${_output_header})
        MACRO_ADD_FILE_DEPENDENCIES(${_output_body})
    endforeach (_i ${_lists})
endmacro(glib_gen_marchal)

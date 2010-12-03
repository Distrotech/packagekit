include(UsePkgConfig)
include(MacroAddFileDependencies)
macro(dbus_gen_binding _sources _xml_interface _prefix _mode)
    get_filename_component(_xml_interface_file ${_xml_interface} ABSOLUTE)
    string(REGEX REPLACE "\\.xml$" ".h" _header_name ${_xml_interface})
    set(_header_name ${CMAKE_CURRENT_SOURCE_DIR}/${_header_name})

    add_custom_command(
        OUTPUT ${_header_name}
        COMMAND dbus-binding-tool --prefix=${_prefix} --mode=${_mode} --output=${_header_name} ${_xml_interface_file}
    )

    set(${_sources} ${${_sources}} ${_header_name})
    MACRO_ADD_FILE_DEPENDENCIES(${_header_name})
endmacro(dbus_gen_binding)

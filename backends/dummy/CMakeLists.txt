pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.22.0 gobject-2.0)

add_definitions(
    -DPK_COMPILATION
)

include_directories(
    ${GLIB2_INCLUDE_DIRS}
)

set(backend_dummy_SRCS
    pk-backend-dummy.c
)

add_plugin(pk_backend_dummy STATIC
    ${backend_dummy_SRCS} 
)

target_link_libraries(pk_backend_dummy
    ${GLIB2_LIBRARIES}
)

# set_target_properties(pk_backend_dummy PROPERTIES
#                       VERSION ${PACKAGEKIT_GLIB_VERSION}
#                       SOVERSION ${PACKAGEKIT_GLIB_SOVERSION}
# )


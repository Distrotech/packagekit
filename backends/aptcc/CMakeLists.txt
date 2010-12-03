# pkg_check_modules(APT_PKG REQUIRED apt-pkg)
# 
# # pkg_check_modules(DBUSGLIB REQUIRED dbus-glib-1>=0.74)
# 
# add_definitions(
#     ${GLIB2_DEFINITIONS}
#     -DPK_COMPILATION
#     -DLOCALSTATEDIR=\"${LOCALSTATEDIR}\"
# )
# 
# include_directories(
#     ${APT_PKG_INCLUDE_DIRS}
# )
# 
# set(libpackagekit_glib_SRCS
#     aptcc_show_error.cpp
#     aptcc_show_broken.cpp
#     apt-utils.cpp
#     matcher.cpp
#     acqprogress.cpp
#     pkg_acqfile.cpp
#     rsources.cpp
#     apt.cpp
#     pk-backend-aptcc.cpp
# )
# 
# include(PkgConfigGlibGenMarchal)
# glib_gen_marchal(libpackagekit_glib_SRCS
#     pk-marshal.list
#     pk_marshal
# )
# 
# add_library(packagekitglib
#     ${libpackagekit_glib_SRCS}
# )
# 
# target_link_libraries(packagekitglib
#     ${GLIB2_LIBRARIES}
#     ${DBUSGLIB_LIBRARIES}
# )
# 
# set_target_properties(packagekitglib PROPERTIES
#                       VERSION ${PACKAGEKIT_GLIB_VERSION}
#                       SOVERSION ${PACKAGEKIT_GLIB_SOVERSION}
# )
# 

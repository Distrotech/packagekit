project(libpackagekit-glib2)

set(PACKAGEKIT_GLIB_SOVERSION 14)
set(PACKAGEKIT_GLIB_REVISION   3)
set(PACKAGEKIT_GLIB_VERSION ${PACKAGEKIT_GLIB_SOVERSION}.${PACKAGEKIT_GLIB_REVISION}.0)

pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.22.0 gobject-2.0)

pkg_check_modules(DBUSGLIB REQUIRED dbus-glib-1>=0.74)

add_definitions(
    ${GLIB2_DEFINITIONS}
    -DPK_COMPILATION
    -DLOCALSTATEDIR=\"${LOCALSTATEDIR}\"
)

include_directories(
    ${GLIB2_INCLUDE_DIR}
    ${DBUSGLIB_INCLUDE_DIRS}
)

set(libpackagekit_glib_SRCS
    egg-debug.c
    egg-string.c
    pk-bitfield.c
    pk-catalog.c
    pk-category.c
    pk-client.c
    pk-client-sync.c
    pk-common.c
    pk-control.c
    pk-control-sync.c
    pk-desktop.c
    pk-details.c
    pk-distro-upgrade.c
    pk-enum.c
    pk-error.c
    pk-eula-required.c
    pk-files.c
    pk-media-change-required.c
    pk-message.c
    pk-package.c
    pk-package-id.c
    pk-package-ids.c
    pk-package-sack.c
    pk-package-sack-sync.c
    pk-progress.c
    pk-repo-detail.c
    pk-repo-signature-required.c
    pk-require-restart.c
    pk-results.c
    pk-service-pack.c
    pk-source.c
    pk-task.c
    pk-transaction-past.c
    pk-transaction-list.c
    pk-update-detail.c
)

include(PkgConfigGlibGenMarchal)
glib_gen_marchal(libpackagekit_glib_SRCS
    pk-marshal.list
    pk_marshal
)

add_library(packagekitglib
    ${libpackagekit_glib_SRCS}
)

target_link_libraries(packagekitglib
    ${GLIB2_LIBRARIES}
    ${DBUSGLIB_LIBRARIES}
)

set_target_properties(packagekitglib PROPERTIES
                      VERSION ${PACKAGEKIT_GLIB_VERSION}
                      SOVERSION ${PACKAGEKIT_GLIB_SOVERSION}
)


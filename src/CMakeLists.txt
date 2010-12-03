find_package(PkgConfig)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.22.0 gobject-2.0)

pkg_check_modules(DBUS REQUIRED dbus-1>=1.1.1)

pkg_check_modules(GIO_UNIX REQUIRED gio-unix-2.0>=2.16.1)

pkg_check_modules(SQLITE REQUIRED sqlite3)

add_definitions(
    ${GLIB_DEFINITIONS}
    -DPK_COMPILATION
    -DLOCALSTATEDIR=\"${LOCALSTATEDIR}\"
    -DLIBDIR=\"${LIBDIR}\"
    -DSYSCONFDIR=\"${SYSCONFDIR}\"
    -DSBINDIR=\"${SBINDIR}\"
    -DDATADIR=\"${DATADIR}\"
    -DPK_DB_DIR=\"${PK_DB_DIR}\"
    -DPACKAGE_LOCALE_DIR=\"${CMAKE_INSTALL_PREFIX}/${DATADIR}/locale\"
)

include_directories(
    ${GLIB2_INCLUDE_DIR}
    ${DBUSGLIB_INCLUDE_DIR}
    ${DBUS_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ../lib
)

set(packagekitd_SRCS
    egg-debug.c
    egg-string.c
    egg-dbus-monitor.c
    pk-marshal.c
    pk-lsof.c
    pk-proc.c
    pk-dbus.c
    pk-transaction.c
    pk-transaction-extra.c
    pk-backend.c
    pk-network.c
    pk-shared.c
    pk-time.c
    pk-network-stack.c
    pk-network-stack-unix.c
    pk-syslog.c
    pk-conf.c
    pk-store.c
    pk-cache.c
    pk-notify.c
    pk-spawn.c
    pk-sysdep.c
    pk-file-monitor.c
    pk-engine.c
    pk-inhibit.c
    pk-backend-spawn.c
    pk-transaction-db.c
    pk-transaction-list.c
    pk-main.c
)

include(PkgConfigGlibGenMarchal)
glib_gen_marchal(packagekitd_SRCS
    pk-marshal.list
    pk_marshal
)

include(PkgConfigDBusBinding)
dbus_gen_binding(packagekitd_SRCS
    org.freedesktop.PackageKit.xml
    pk_engine
    glib-server
)
dbus_gen_binding(packagekitd_SRCS
    org.freedesktop.PackageKit.Transaction.xml
    pk_transaction
    glib-server
)

add_executable(packagekitd
    ${packagekitd_SRCS}
)

target_link_libraries(packagekitd
    ${GLIB2_LIBRARIES}
    ${DBUSGLIB_LIB}
    ${GIO_UNIX_LIBRARIES}
    ${SQLITE_LIBRARIES}
    packagekitglib
)

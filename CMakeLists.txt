project(PackageKit)

cmake_minimum_required(VERSION 2.6.0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/)

include(FindPkgConfig)

# Set's the var dir
set(LOCALSTATEDIR ${CMAKE_INSTALL_PREFIX}/var)
set(LIBDIR ${CMAKE_INSTALL_PREFIX}/lib)
set(SYSCONFDIR ${CMAKE_INSTALL_PREFIX}/etc)
set(SBINDIR ${CMAKE_INSTALL_PREFIX}/sbin)
set(DATADIR ${CMAKE_INSTALL_PREFIX}/share)
set(PK_DB_DIR ${LOCALSTATEDIR}/lib/PackageKit)

set(GETTEXT_PACKAGE PackageKit)

set(PACKAGE_VERSION "0.6.8")
set(VERSION ${PACKAGE_VERSION})

# Generate config.h
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_subdirectory(lib)
add_subdirectory(src)

# Backends
option(BACKEND_ALPM    "Build ALPM backend" OFF)
option(BACKEND_APT     "Build APT backend" OFF)
option(BACKEND_APTCC   "Build APTcc backend" OFF)
option(BACKEND_BOX     "Build BOX backend" OFF)
option(BACKEND_CONARY  "Build CONARY backend" OFF)
option(BACKEND_DUMMY   "Build dummy backend" OFF)
option(BACKEND_ENTROPY "Build Entropy backend" OFF)
option(BACKEND_OPKG    "Build OPKG backend" OFF)
option(BACKEND_RAZOR   "Build Razor backend" OFF)
option(BACKEND_PACMAN  "Build Pacman backend" OFF)
option(BACKEND_PISI    "Build PiSi backend" OFF)
option(BACKEND_POLDEK  "Build poldek backend" OFF)
option(BACKEND_PORTAGE "Build Portage backend" OFF)
option(BACKEND_PORTS   "Build Ports backend" OFF)
option(BACKEND_SLAPT   "Build Slapt backend" OFF)
option(BACKEND_SMART   "Build SMART backend" OFF)
option(BACKEND_URPMI   "Build URPMI backend" OFF)
option(BACKEND_YUM     "Build YUM backend" OFF)
option(BUILD_ZIF       "Build YUM with zif" OFF)
option(BACKEND_ZYPP    "Build Zypp backend" OFF)
option(DEFAULT_BACKEND "Defaul backend")

add_subdirectory(backends)


message(STATUS "
=========================================================================
                   PackageKit ${VERSION}
                  =====================


                    prefix:   ${CMAKE_INSTALL_PREFIX}
                   datadir:   ${datadir}
       Building unit tests:   ${BUILD_tests}
       Able to run locally:   ${RUN_LOCAL}
        Security framework:   ${SECURITY_FRAMEWORK}
         Networking stacks:   ${networking_apis}
            Browser plugin:   ${build_browser_plugin}
          GStreamer plugin:   ${build_gstreamer_plugin}
       Pango module (GTK2):   ${build_gtk_module_gtk2}
       Pango module (GTK3):   ${build_gtk_module_gtk3}
    BASH Command not found:   ${build_command_not_found}
              Cron scripts:   ${build_cron}
         debuginfo-install:   ${build_debuginfo_install}
             device-rebind:   ${build_device_rebind}
          pm-utils scripts:   ${build_pm_utils}
                QT library:   ${build_qt}
          Managed bindings:   ${build_managed}

    Backends:
              ALPM backend:  ${BACKEND_ALPM}
               APT backend:  ${BACKEND_APT}
             APTcc backend:  ${BACKEND_APTCC}
               BOX backend:  ${BACKEND_BOX}
            CONARY backend:  ${BACKEND_CONARY}
             dummy backend:  ${BACKEND_DUMMY}
           Entropy backend:  ${BACKEND_ENTROPY}
              OPKG backend:  ${BACKEND_OPKG}
             Razor backend:  ${BACKEND_RAZOR}
            Pacman backend:  ${BACKEND_PACMAN}
              PiSi backend:  ${BACKEND_PISI}
            poldek backend:  ${BACKEND_POLDEK}
           Portage backend:  ${BACKEND_PORTAGE}
             Ports backend:  ${BACKEND_PORTS}
             Slapt backend:  ${BACKEND_SLAPT}
             SMART backend:  ${BACKEND_SMART}
             URPMI backend:  ${BACKEND_URPMI}
               YUM backend:  ${BACKEND_YUM}
                (with zif):  ${BUILD_ZIF}
              Zypp backend:  ${BACKEND_ZYPP}

           Default backend:  ${DEFAULT_BACKEND}
")
#ifndef CONFIG_H
#define CONFIG_H

/* default backend prefix */
#cmakedefine DEFAULT_BACKEND

/* Build test code */
#cmakedefine EGG_BUILD_TESTS

/* always defined to indicate that i18n is enabled */
#cmakedefine ENABLE_NLS

/* Name of default gettext domain */
#cmakedefine GETTEXT_PACKAGE "@GETTEXT_PACKAGE@"

/* Define to 1 if you have the <archive.h> header file. */
#cmakedefine HAVE_ARCHIVE_H

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
#cmakedefine HAVE_BIND_TEXTDOMAIN_CODESET

/* Define to 1 if you have the `clearenv' function. */
#cmakedefine HAVE_CLEARENV

/* Define to 1 if you have the `dcgettext' function. */
#cmakedefine HAVE_DCGETTEXT

/* Set to true if apt is DDTP-enabled */
#cmakedefine HAVE_DDTP

/* Define to 1 if you have the <dlfcn.h> header file. */
#cmakedefine HAVE_DLFCN_H

/* Define to 1 if you have the <execinfo.h> header file. */
#cmakedefine HAVE_EXECINFO_H

/* Define if the GNU gettext() function is already present or preinstalled. */
#cmakedefine HAVE_GETTEXT

/* Define to 1 if you have the <inttypes.h> header file. */
#cmakedefine HAVE_INTTYPES_H

/* Define if your <locale.h> file defines LC_MESSAGES. */
#cmakedefine HAVE_LC_MESSAGES

/* Define to 1 if you have the `apt-pkg' library (-lapt-pkg). */
#cmakedefine HAVE_LIBAPT_PKG

/* Define to 1 if you have the <locale.h> header file. */
#cmakedefine HAVE_LOCALE_H

/* Define to 1 if you have the <memory.h> header file. */
#cmakedefine HAVE_MEMORY_H

/* "Meta release is available" */
#cmakedefine HAVE_PYTHON_META_RELEASE

/* "Python software properties is available" */
#cmakedefine HAVE_PYTHON_SOFTWARE_PROPERTIES

/* Define to 1 if you have the `setpriority' function. */
#cmakedefine HAVE_SETPRIORITY

/* Define to 1 if you have the <stdint.h> header file. */
#cmakedefine HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#cmakedefine HAVE_STDLIB_H

/* Define to 1 if you have the <strings.h> header file. */
#cmakedefine HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#cmakedefine HAVE_STRING_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#cmakedefine HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/types.h> header file. */
#cmakedefine HAVE_SYS_TYPES_H

/* Define to 1 if you have the <unistd.h> header file. */
#cmakedefine HAVE_UNISTD_H

/* If Zif support should be enabled */
#cmakedefine HAVE_ZIF

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#cmakedefine LT_OBJDIR

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
#cmakedefine NO_MINUS_C_MINUS_O

/* Name of package */
#cmakedefine PACKAGE

/* User for running the PackageKit daemon */
#cmakedefine PACKAGEKIT_USER

/* Define to the address where bug reports for this package should be sent. */
#cmakedefine PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#cmakedefine PACKAGE_NAME

/* Define to the full name and version of this package. */
#cmakedefine PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#cmakedefine PACKAGE_TARNAME

/* Define to the home page for this package. */
#cmakedefine PACKAGE_URL

/* Define to the version of this package. */
#cmakedefine PACKAGE_VERSION "@PACKAGE_VERSION@"

/* define if Connection Manager is installed */
#cmakedefine PK_BUILD_CONNMAN

/* Build local code */
#cmakedefine PK_BUILD_LOCAL

/* define if NetworkManager is installed */
#cmakedefine PK_BUILD_NETWORKMANAGER

/* Define to 1 if GPGME is available to Slapt */
#cmakedefine SLAPT_HAS_GPGME

/* Define to 1 if you have the ANSI C header files. */
#cmakedefine STDC_HEADERS

/* if we should use a dummy security framework */
#cmakedefine USE_SECURITY_DUMMY

/* if we should use PolicyKit */
#cmakedefine USE_SECURITY_POLKIT

/* if we should use PolicyKit's new API */
#cmakedefine USE_SECURITY_POLKIT_NEW

/* Version number of package */
#cmakedefine VERSION "@VERSION@"

/* default security framework */
#cmakedefine security_framework


#endif //CONFIG_H
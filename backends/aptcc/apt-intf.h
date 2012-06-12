/* apt-intf.h - Interface to APT
 *
 * Copyright (c) 1999-2002, 2004-2005, 2007-2008 Daniel Burrows
 * Copyright (c) 2009 Daniel Nicoletti <dantti12@gmail.com>
 *               2012 Matthias Klumpp <matthias@tenstral.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef APTINTF_H
#define APTINTF_H

#include <glib.h>
#include <glib/gstdio.h>

#include <apt-pkg/depcache.h>
#include <apt-pkg/acquire.h>

#include <pk-backend.h>

#include "PkgList.h"

#define PREUPGRADE_BINARY    "/usr/bin/do-release-upgrade"
#define GDEBI_BINARY         "/usr/bin/gdebi"
#define REBOOT_REQUIRED      "/var/run/reboot-required"

class pkgProblemResolver;
class Matcher;
class AptCacheFile;
class AptIntf
{
public:
    AptIntf(PkBackend *backend, bool &cancel);
    ~AptIntf();

    bool init();
    void cancel();

    /**
     * Tries to find a package with the given packageId
     * @returns pkgCache::VerIterator, if .end() is true the package could not be found
     */
    pkgCache::VerIterator findPackageId(const gchar *packageId);

    /**
     * Tries to find the current version of a package
     * if it can't find it will return the candidate
     * TODO check if we really need the candidate version
     * @returns pkgCache::VerIterator, if .end() is true the version could not be found
     */
    pkgCache::VerIterator findVer(const pkgCache::PkgIterator &pkg);

    /**
     * Tries to find a list of packages mathing the package ids
     * @returns a list of pkgCache::VerIterator, if the list is empty no package was found
     */
    PkgList resolvePackageIds(gchar **package_ids, PkBitfield filters = PK_FILTER_ENUM_NONE);

    /**
      * Refreshes the sources of packages
      */
    void refreshCache();

    /**
      * Tries to resolve a pkg file installation of the given \sa file
      * @param install is where the packages to be installed will be stored
      * @param remove is where the packages to be removed will be stored
      * @returns true if the package can be installed
      */
    bool markFileForInstall(const gchar *file, PkgList &install, PkgList &remove);

    /**
      * Marks the given packages as auto installed
      */
    void markAutoInstalled(AptCacheFile &cache, const PkgList &pkgs);

    /**
     *  runs a transaction to install/remove/update packages
     *  - for install and update, \p remove should be set to false
     *  - if you are going to remove, \p remove should be true
     *  - if you don't want to actually install/update/remove
     *    \p simulate should be true, in this case packages with
     *    what's going to happen will be emitted.
     */
    bool runTransaction(const PkgList &install,
                        const PkgList &remove,
                        bool simulate,
                        bool markAuto,
                        bool fixBroken,
                        bool downloadOnly);

    /**
     *  Get package depends
     */
    void getDepends(PkgList &output,
                    const pkgCache::VerIterator &ver,
                    bool recursive);

    /**
     *  Get package requires
     */
    void getRequires(PkgList &output,
                     const pkgCache::VerIterator &ver,
                     bool recursive);

    /**
      * Returns a list of all packages in the cache
      */
    PkgList getPackages();

    /**
      * Returns a list of all packages in the given groups
      */
    PkgList getPackagesFromGroup(gchar **values);

    /**
      * Returns a list of all packages that matched their names with matcher
      */
    PkgList searchPackageName(gchar *search);

    /**
      * Returns a list of all packages that matched their description with matcher
      */
    PkgList searchPackageDetails(gchar *search);

    /**
      * Returns a list of all packages that matched contains the given files
      */
    PkgList searchPackageFiles(gchar **values);

    /**
     *  Emits a package with the given state
     */
    void emitPackage(const pkgCache::VerIterator &ver, PkInfoEnum state = PK_INFO_ENUM_UNKNOWN);

    /**
     *  Emits a package with the given percentage
     */
    void emitPackageProgress(const pkgCache::VerIterator &ver, uint percentage);

    /**
      * Emits a list of packages that matches the given filters
      */
    void emitPackages(PkgList &output,
                      PkBitfield filters = PK_FILTER_ENUM_NONE,
                      PkInfoEnum state = PK_INFO_ENUM_UNKNOWN);

    void emitRequireRestart(PkgList &output);

    /**
      * Emits a list of updates that matches the given filters
      */
    void emitUpdates(PkgList &output, PkBitfield filters = PK_FILTER_ENUM_NONE);

    /**
      * Checks if a given package matches the filters
      * @returns true if it passed the filters
      */
    bool matchPackage(const pkgCache::VerIterator &ver, PkBitfield filters);

    /**
      * Returns the list of packages with the ones that passed the given filters
      */
    PkgList filterPackages(const PkgList &packages, PkBitfield filters);

    /**
      * Emits details of the given package
      */
    void emitPackageDetail(const pkgCache::VerIterator &ver);

    /**
      * Emits details of the given package list
      */
    void emitDetails(PkgList &pkgs);

    /**
      * Emits update detail
      */
    void emitUpdateDetail(const pkgCache::VerIterator &candver);

    /**
      * Emits update datails for the given list
      */
    void emitUpdateDetails(const PkgList &pkgs);

    /**
      *  Emits the files of a package
      */
    void emitPackageFiles(const gchar *pi);

    /**
      *  Download and install packages
      */
    bool installPackages(AptCacheFile &cache, bool simulating, bool downloadOnly);

    /**
     *  Install a DEB file
     *
     *  If you don't want to actually install/update/remove
     *    \p simulate should be true, in this case packages with
     *    what's going to happen will be emitted.
     */
    bool installFile(const gchar *path, bool simulate);

    /**
     *  Check which package provides the codec
     */
    void providesCodec(PkgList &output, gchar **values);

    /**
     *  Check which package provides a shared library
     */
    void providesLibrary(PkgList &output, gchar **values);

    /**
     *  returns a list of packages names
     */
    void providesMimeType(PkgList &output, gchar **values);

    /** Like pkgAcqArchive, but uses generic File objects to download to
     *  the cwd (and copies from file:/ URLs).
     */
    bool getArchive(pkgAcquire *Owner, pkgCache::VerIterator const &Version,
                    std::string directory, std::string &StoreFilename);

private:
    bool checkTrusted(pkgAcquire &fetcher, bool simulating);
    bool packageIsSupported(const pkgCache::VerIterator &verIter, string component);
    void tryToRemove(const pkgCache::VerIterator &ver,
                     pkgDepCache &Cache,
                     pkgProblemResolver &Fix);
    bool tryToInstall(const pkgCache::VerIterator &ver,
                      pkgDepCache &Cache,
                      pkgProblemResolver &Fix,
                      bool BrokenFix,
                      unsigned int &ExpectedInst);

    /**
     *  interprets dpkg status fd
     */
    void updateInterface(int readFd, int writeFd);
    bool doAutomaticRemove(AptCacheFile &cache);
    bool removingEssentialPackages(AptCacheFile &cache);
    PkgList checkChangedPackages(AptCacheFile &cache, bool emitChanged);
    pkgCache::VerIterator findTransactionPackage(const std::string &name);

    AptCacheFile *m_cache;
    PkBackend  *m_backend;
    bool       &m_cancel;
    struct stat m_restartStat;

    bool m_isMultiArch;
    PkgList m_pkgs;
    PkgList m_restartPackages;

    time_t     m_lastTermAction;
    string     m_lastPackage;
    uint       m_lastSubProgress;
    bool       m_startCounting;

    // when the internal terminal timesout after no activity
    int m_terminalTimeout;
    pid_t m_child_pid;
};

#endif

#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Provides an apt backend to PackageKit

Copyright (C) 2013 Sebastian Heinlein <glatzor@ubuntu.com>

Licensed under the GNU General Public License Version 2

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""

__author__ = "Sebastian Heinlein <devel@glatzor.de>"


import sys

from clickpackage.install import ClickInstaller
from packagekit import backend, enums


CLICK_ROOT = "/opt/click.ubuntu.com"


class ClickPackageKitBackend(backend.PackageKitBaseBackend):

    """This class provides a PackageKit backend for the click
    packages introduced in the Ubuntu Phone.
    """

    def install_files(self, transaction_flags, install_files):
        """Install downloaded package files."""
        self.percentage(None)
        self.status(enums.STATUS_INSTALL)
        self.allow_cancel(False)

        installer = ClickInstaller(root=CLICK_ROOT,
                                   force_missing_framework=False)

        for file_path in install_files:
            try:
                pkg_name, pkg_version = installer.audit(file_path)
            except Exception as error:
                self.error(enums.ERROR_INVALID_PACKAGE_FILE, str(error))
                break
            self.package("%s;%s;;" % (pkg_name, pkg_version),
                         enums.INFO_INSTALLING,
                         "")
            try:
                installer.install(file_path)
            except Exception as error:
                self.error(enums.ERROR_INTERNAL_ERROR, str(error))
                break
            self.package("%s;%s;;" % (pkg_name, pkg_version),
                         enums.INFO_INSTALLED,
                         "")

def main():
   backend = ClickPackageKitBackend("")
   backend.dispatcher(sys.argv[1:])

if __name__ == "__main__":
    main()

# vim: es=4 et sts=4

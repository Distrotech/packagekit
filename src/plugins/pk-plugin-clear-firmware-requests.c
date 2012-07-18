/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>
#include <gio/gio.h>
#include <pk-plugin.h>

#include "pk-shared.h"

/**
 * pk_plugin_get_description:
 */
const gchar *
pk_plugin_get_description (void)
{
	return "Clears firmware requests";
}

/**
 * pk_plugin_transaction_finished_end:
 */
void
pk_plugin_transaction_finished_end (PkPlugin *plugin,
				    PkTransaction *transaction)
{
	gboolean ret;
	gchar *filename = NULL;
	PkRoleEnum role;

	/* only do this for RefreshCache, usually once a week */
	role = pk_transaction_get_role (transaction);
	if (role != PK_ROLE_ENUM_REFRESH_CACHE)
		goto out;

	/* skip simulate actions */
	if (pk_bitfield_contain (pk_transaction_get_transaction_flags (transaction),
				 PK_TRANSACTION_FLAG_ENUM_SIMULATE)) {
		goto out;
	}

	/* skip only-download */
	if (pk_bitfield_contain (pk_transaction_get_transaction_flags (transaction),
				 PK_TRANSACTION_FLAG_ENUM_ONLY_DOWNLOAD)) {
		goto out;
	}

	/* clear the firmware requests directory */
	filename = g_build_filename (LOCALSTATEDIR, "run", "PackageKit", "udev", NULL);
	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
		goto out;
	g_debug ("clearing udev firmware requests at %s", filename);
	ret = pk_directory_remove_contents (filename);
	if (!ret)
		g_warning ("failed to clear %s", filename);
out:
	g_free (filename);
}

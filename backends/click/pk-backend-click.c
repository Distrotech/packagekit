/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2010 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2013 Sebastian Heinlein <devel@glatzor.de>
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
#include <pk-backend.h>
#include <pk-backend-spawn.h>
#include <string.h>

typedef struct {
	PkBackendSpawn	*spawn;
} PkBackendClickPrivate;

static PkBackendClickPrivate *priv;

/**
 * pk_backend_get_description:
 */
const gchar *
pk_backend_get_description (PkBackend *backend)
{
	return "Click";
}

/**
 * pk_backend_get_author:
 */
const gchar *
pk_backend_get_author (PkBackend *backend)
{
	return "Sebastian Heinlein <devel@glatzor.de>";
}

/**
 * pk_backend_stderr_cb:
 */
static gboolean
pk_backend_stderr_cb (PkBackendJob *job, const gchar *output)
{
	if (strstr (output, "DeprecationWarning") != NULL)
		return FALSE;
	return TRUE;
}

/**
 * pk_backend_stdout_cb:
 */
static gboolean
pk_backend_stdout_cb (PkBackendJob *job, const gchar *output)
{
	return TRUE;
}


/**
 * pk_backend_initialize:
 * This should only be run once per backend load, i.e. not every transaction
 */
void
pk_backend_initialize (PkBackend *backend)
{
	/* create private area */
	priv = g_new0 (PkBackendClickPrivate, 1);

	g_debug ("backend: initialize");
	priv->spawn = pk_backend_spawn_new ();
	pk_backend_spawn_set_filter_stderr (priv->spawn, pk_backend_stderr_cb);
	pk_backend_spawn_set_filter_stdout (priv->spawn, pk_backend_stdout_cb);
	pk_backend_spawn_set_name (priv->spawn, "click");
	pk_backend_spawn_set_allow_sigkill (priv->spawn, FALSE);
}

/**
 * pk_backend_destroy:
 * This should only be run once per backend load, i.e. not every transaction
 */
void
pk_backend_destroy (PkBackend *backend)
{
	g_debug ("backend: destroy");
	g_object_unref (priv->spawn);
	g_free (priv);
}

/**
 * pk_backend_start_job:
 */
void
pk_backend_start_job (PkBackend *backend, PkBackendJob *job)
{
	if (pk_backend_spawn_is_busy (priv->spawn)) {
		pk_backend_job_error_code (job,
					   PK_ERROR_ENUM_LOCK_REQUIRED,
					   "spawned backend requires lock");
		pk_backend_job_finished (job);
		return;
	}
}

/**
 * pk_backend_get_filters:
 */
PkBitfield
pk_backend_get_filters (PkBackend *backend)
{
	return pk_bitfield_from_enums (
		PK_FILTER_ENUM_INSTALLED,
		-1);
}

/**
 * pk_backend_get_roles:
 */
PkBitfield
pk_backend_get_roles (PkBackend *backend)
{
	return pk_bitfield_from_enums (
		PK_ROLE_ENUM_GET_PACKAGES,
		PK_ROLE_ENUM_INSTALL_FILES,
		PK_ROLE_ENUM_REMOVE_PACKAGES,
		-1);
}

/**
 * pk_backend_get_mime_types:
 */
gchar **
pk_backend_get_mime_types (PkBackend *backend)
{
	const gchar *mime_types[] = {
				"application/x-deb",
				NULL };
	return g_strdupv ((gchar **) mime_types);
}

/**
 *  * pk_backend_get_groups:
 *   */
PkBitfield
pk_backend_get_groups (PkBackend *backend)
{
	return pk_bitfield_from_enums (
		PK_GROUP_ENUM_UNKNOWN,
		-1);
}

/**
 * pk_backend_get_packages:
 */
void
pk_backend_get_packages (PkBackend *backend, PkBackendJob *job, PkBitfield filters)
{
	gchar *filters_text;
	filters_text = pk_filter_bitfield_to_string (filters);
	pk_backend_spawn_helper (priv->spawn, job, "clickBackend.py", "get-packages", filters_text, NULL);
	g_free (filters_text);
}

/**
 * pk_backend_install_files:
 */
void
pk_backend_install_files (PkBackend *backend, PkBackendJob *job, PkBitfield transaction_flags, gchar **full_paths)
{
	gchar *package_ids_temp;
	gchar *transaction_flags_temp;
	package_ids_temp = g_strjoinv (PK_BACKEND_SPAWN_FILENAME_DELIM, full_paths);
	transaction_flags_temp = pk_transaction_flag_bitfield_to_string (transaction_flags);
	pk_backend_spawn_helper (priv->spawn, job,
				 "clickBackend.py",
				 "install-files",
				 transaction_flags_temp,
				 package_ids_temp,
				 NULL);
	g_free (package_ids_temp);
	g_free (transaction_flags_temp);
}

/**
 * pk_backend_remove_packages:
 */
void
pk_backend_remove_packages (PkBackend *backend, PkBackendJob *job,
			    PkBitfield transaction_flags,
			    gchar **package_ids,
			    gboolean allow_deps,
			    gboolean autoremove)
{
	gchar *package_ids_temp;
	gchar *transaction_flags_temp;
	package_ids_temp = pk_package_ids_to_string (package_ids);
	transaction_flags_temp = pk_transaction_flag_bitfield_to_string (transaction_flags);
	pk_backend_spawn_helper (priv->spawn, job,
				 "clickBackend.py",
				 "remove-packages",
				 transaction_flags_temp,
				 package_ids_temp,
				 pk_backend_bool_to_string (allow_deps),
				 pk_backend_bool_to_string (autoremove),
				 NULL);
	g_free (package_ids_temp);
	g_free (transaction_flags_temp);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2008 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gmodule.h>
#include <packagekit-glib2/pk-enum.h>
#include <packagekit-glib2/pk-common.h>
#include <packagekit-glib2/pk-package-id.h>

#include "egg-debug.h"
#include "egg-string.h"

#include "pk-backend-internal.h"
#include "pk-backend-spawn.h"
#include "pk-marshal.h"
#include "pk-spawn.h"
#include "pk-shared.h"
#include "pk-time.h"
#include "pk-inhibit.h"
#include "pk-conf.h"

#define PK_BACKEND_SPAWN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PK_TYPE_BACKEND_SPAWN, PkBackendSpawnPrivate))
#define PK_BACKEND_SPAWN_PERCENTAGE_INVALID	101

struct PkBackendSpawnPrivate
{
	PkSpawn			*spawn;
	PkBackend		*backend;
	gchar			*name;
	guint			 kill_id;
	PkConf			*conf;
	gboolean		 finished;
	gboolean		 allow_sigkill;
	PkBackendSpawnFilterFunc stdout_func;
	PkBackendSpawnFilterFunc stderr_func;
};

G_DEFINE_TYPE (PkBackendSpawn, pk_backend_spawn, G_TYPE_OBJECT)

/**
 * pk_backend_spawn_set_filter_stdout:
 **/
gboolean
pk_backend_spawn_set_filter_stdout (PkBackendSpawn *backend_spawn, PkBackendSpawnFilterFunc func)
{
	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), FALSE);
	backend_spawn->priv->stdout_func = func;
	return TRUE;
}

/**
 * pk_backend_spawn_set_filter_stderr:
 **/
gboolean
pk_backend_spawn_set_filter_stderr (PkBackendSpawn *backend_spawn, PkBackendSpawnFilterFunc func)
{
	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), FALSE);
	backend_spawn->priv->stderr_func = func;
	return TRUE;
}

/**
 * pk_backend_spawn_exit_timeout_cb:
 **/
static gboolean
pk_backend_spawn_exit_timeout_cb (PkBackendSpawn *backend_spawn)
{
	gboolean ret;

	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), FALSE);

	/* only try to close if running */
	ret = pk_spawn_is_running (backend_spawn->priv->spawn);
	if (ret) {
		egg_debug ("closing dispatcher as running and is idle");
		pk_spawn_exit (backend_spawn->priv->spawn);
	}
	return FALSE;
}

/**
 * pk_backend_spawn_start_kill_timer:
 **/
static void
pk_backend_spawn_start_kill_timer (PkBackendSpawn *backend_spawn)
{
	gint timeout;
	PkBackendSpawnPrivate *priv = backend_spawn->priv;

	/* we finished okay, so we don't need to emulate Finished() for a crashing script */
	priv->finished = TRUE;
	egg_debug ("backend marked as finished, so starting kill timer");

	if (priv->kill_id > 0)
		g_source_remove (priv->kill_id);

	/* get policy timeout */
	timeout = pk_conf_get_int (priv->conf, "BackendShutdownTimeout");
	if (timeout == PK_CONF_VALUE_INT_MISSING) {
		egg_warning ("using built in default value");
		timeout = 5;
	}

	/* close down the dispatcher if it is still open after this much time */
	priv->kill_id = g_timeout_add_seconds (timeout, (GSourceFunc) pk_backend_spawn_exit_timeout_cb, backend_spawn);
#if GLIB_CHECK_VERSION(2,25,8)
	g_source_set_name_by_id (priv->kill_id, "[PkBackendSpawn] exit");
#endif
}

/**
 * pk_backend_spawn_parse_stdout:
 **/
static gboolean
pk_backend_spawn_parse_stdout (PkBackendSpawn *backend_spawn, const gchar *line)
{
	gchar **sections;
	guint size;
	gchar *command;
	gchar *text;
	gboolean ret = TRUE;
	PkInfoEnum info;
	PkRestartEnum restart;
	PkGroupEnum group;
	gulong package_size;
	gint percentage;
	PkErrorEnum error_enum;
	PkStatusEnum status_enum;
	PkMessageEnum message_enum;
	PkRestartEnum restart_enum;
	PkSigTypeEnum sig_type;
	PkUpdateStateEnum update_state_enum;
	PkMediaTypeEnum media_type_enum;
	PkDistroUpgradeEnum distro_upgrade_enum;
	PkBackendSpawnPrivate *priv = backend_spawn->priv;

	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), FALSE);

	/* check if output line */
	if (line == NULL)
		return FALSE;

	/* split by tab */
	sections = g_strsplit (line, "\t", 0);
	command = sections[0];

	/* get size */
	size = g_strv_length (sections);

	if (g_strcmp0 (command, "package") == 0) {
		if (size != 4) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		if (pk_package_id_check (sections[2]) == FALSE) {
			egg_warning ("invalid package_id");
			ret = FALSE;
			goto out;
		}
		info = pk_info_enum_from_string (sections[1]);
		if (info == PK_INFO_ENUM_UNKNOWN) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "Info enum not recognised, and hence ignored: '%s'", sections[1]);
			ret = FALSE;
			goto out;
		}
		pk_backend_package (priv->backend, info, sections[2], sections[3]);
	} else if (g_strcmp0 (command, "details") == 0) {
		if (size != 7) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		group = pk_group_enum_from_string (sections[3]);

		/* ITS4: ignore, checked for overflow */
		package_size = atol (sections[6]);
		if (package_size > 1073741824) {
			egg_warning ("package size cannot be larger than one Gb");
			ret = FALSE;
			goto out;
		}
		text = g_strdup (sections[4]);
		/* convert ; to \n as we can't emit them on stdout */
		g_strdelimit (text, ";", '\n');
		pk_backend_details (priv->backend, sections[1], sections[2],
					group, text, sections[5], package_size);
		g_free (text);
	} else if (g_strcmp0 (command, "finished") == 0) {
		if (size != 1) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		pk_backend_finished (priv->backend);

		/* from this point on, we can start the kill timer */
		pk_backend_spawn_start_kill_timer (backend_spawn);

	} else if (g_strcmp0 (command, "files") == 0) {
		if (size != 3) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		pk_backend_files (priv->backend, sections[1], sections[2]);
	} else if (g_strcmp0 (command, "repo-detail") == 0) {
		if (size != 4) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		if (g_strcmp0 (sections[3], "true") == 0) {
			pk_backend_repo_detail (priv->backend, sections[1], sections[2], TRUE);
		} else if (g_strcmp0 (sections[3], "false") == 0) {
			pk_backend_repo_detail (priv->backend, sections[1], sections[2], FALSE);
		} else {
			egg_warning ("invalid qualifier '%s'", sections[3]);
			ret = FALSE;
			goto out;
		}
	} else if (g_strcmp0 (command, "updatedetail") == 0) {
		if (size != 13) {
			egg_warning ("invalid command '%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		restart = pk_restart_enum_from_string (sections[7]);
		if (restart == PK_RESTART_ENUM_UNKNOWN) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "Restart enum not recognised, and hence ignored: '%s'", sections[7]);
			ret = FALSE;
			goto out;
		}
		update_state_enum = pk_update_state_enum_from_string (sections[10]);
		/* convert ; to \n as we can't emit them on stdout */
		g_strdelimit (sections[8], ";", '\n');
		g_strdelimit (sections[9], ";", '\n');
		pk_backend_update_detail (priv->backend, sections[1],
					  sections[2], sections[3], sections[4],
					  sections[5], sections[6], restart, sections[8],
					  sections[9], update_state_enum,
					  sections[11], sections[12]);
	} else if (g_strcmp0 (command, "percentage") == 0) {
		if (size != 2) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		ret = egg_strtoint (sections[1], &percentage);
		if (!ret) {
			egg_warning ("invalid percentage value %s", sections[1]);
		} else if (percentage < 0 || percentage > 100) {
			egg_warning ("invalid percentage value %i", percentage);
			ret = FALSE;
		} else {
			pk_backend_set_percentage (priv->backend, percentage);
		}
	} else if (g_strcmp0 (command, "subpercentage") == 0) {
		if (size != 2) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		ret = egg_strtoint (sections[1], &percentage);
		if (!ret) {
			egg_warning ("invalid subpercentage value %s", sections[1]);
		} else if (percentage < 0 || percentage > 100) {
			egg_warning ("invalid subpercentage value %i", percentage);
			ret = FALSE;
		} else {
			pk_backend_set_sub_percentage (priv->backend, percentage);
		}
	} else if (g_strcmp0 (command, "error") == 0) {
		if (size != 3) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		error_enum = pk_error_enum_from_string (sections[1]);
		if (error_enum == PK_ERROR_ENUM_UNKNOWN) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "Error enum not recognised, and hence ignored: '%s'", sections[1]);
			ret = FALSE;
			goto out;
		}
		/* convert back all the ;'s to newlines */
		text = g_strdup (sections[2]);

		/* convert ; to \n as we can't emit them on stdout */
		g_strdelimit (text, ";", '\n');

		/* convert % else we try to format them */
		g_strdelimit (text, "%", '$');

		pk_backend_error_code (priv->backend, error_enum, text);
		g_free (text);
	} else if (g_strcmp0 (command, "requirerestart") == 0) {
		if (size != 3) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		restart_enum = pk_restart_enum_from_string (sections[1]);
		if (restart_enum == PK_RESTART_ENUM_UNKNOWN) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "Restart enum not recognised, and hence ignored: '%s'", sections[1]);
			ret = FALSE;
			goto out;
		}
		if (!pk_package_id_check (sections[2])) {
			egg_warning ("invalid package_id");
			ret = FALSE;
			goto out;
		}
		pk_backend_require_restart (priv->backend, restart_enum, sections[2]);
	} else if (g_strcmp0 (command, "message") == 0) {
		if (size != 3) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		message_enum = pk_message_enum_from_string (sections[1]);
		if (message_enum == PK_MESSAGE_ENUM_UNKNOWN) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "Message enum not recognised, and hence ignored: '%s'", sections[1]);
			ret = FALSE;
			goto out;
		}
		text = g_strdup (sections[2]);
		/* convert ; to \n as we can't emit them on stdout */
		g_strdelimit (text, ";", '\n');
		pk_backend_message (priv->backend, message_enum, text);
		g_free (text);
	} else if (g_strcmp0 (command, "change-transaction-data") == 0) {
		if (size != 2) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		pk_backend_set_transaction_data (priv->backend, sections[1]);
	} else if (g_strcmp0 (command, "status") == 0) {
		if (size != 2) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		status_enum = pk_status_enum_from_string (sections[1]);
		if (status_enum == PK_STATUS_ENUM_UNKNOWN) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "Status enum not recognised, and hence ignored: '%s'", sections[1]);
			ret = FALSE;
			goto out;
		}
		pk_backend_set_status (priv->backend, status_enum);
	} else if (g_strcmp0 (command, "allow-cancel") == 0) {
		if (size != 2) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		if (g_strcmp0 (sections[1], "true") == 0) {
			pk_backend_set_allow_cancel (priv->backend, TRUE);
		} else if (g_strcmp0 (sections[1], "false") == 0) {
			pk_backend_set_allow_cancel (priv->backend, FALSE);
		} else {
			egg_warning ("invalid section '%s'", sections[1]);
			ret = FALSE;
			goto out;
		}
	} else if (g_strcmp0 (command, "no-percentage-updates") == 0) {
		if (size != 1) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		pk_backend_set_percentage (priv->backend, PK_BACKEND_PERCENTAGE_INVALID);
	} else if (g_strcmp0 (command, "repo-signature-required") == 0) {

		if (size != 9) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}

		sig_type = pk_sig_type_enum_from_string (sections[8]);
		if (sig_type == PK_SIGTYPE_ENUM_UNKNOWN) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "Sig enum not recognised, and hence ignored: '%s'", sections[8]);
			ret = FALSE;
			goto out;
		}
		if (egg_strzero (sections[1])) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "package_id blank, and hence ignored: '%s'", sections[1]);
			ret = FALSE;
			goto out;
		}
		if (egg_strzero (sections[2])) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "repository name blank, and hence ignored: '%s'", sections[2]);
			ret = FALSE;
			goto out;
		}

		/* pass _all_ of the data */
		ret = pk_backend_repo_signature_required (priv->backend, sections[1],
							  sections[2], sections[3], sections[4],
							  sections[5], sections[6], sections[7], sig_type);
		goto out;

	} else if (g_strcmp0 (command, "eula-required") == 0) {

		if (size != 5) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}

		if (egg_strzero (sections[1])) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "eula_id blank, and hence ignored: '%s'", sections[1]);
			ret = FALSE;
			goto out;
		}

		if (egg_strzero (sections[2])) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "package_id blank, and hence ignored: '%s'", sections[2]);
			ret = FALSE;
			goto out;
		}

		if (egg_strzero (sections[4])) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "agreement name blank, and hence ignored: '%s'", sections[4]);
			ret = FALSE;
			goto out;
		}

		ret = pk_backend_eula_required (priv->backend, sections[1], sections[2], sections[3], sections[4]);
		goto out;

	} else if (g_strcmp0 (command, "media-change-required") == 0) {

		if (size != 4) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}

		media_type_enum = pk_media_type_enum_from_string (sections[1]);
		if (media_type_enum == PK_MEDIA_TYPE_ENUM_UNKNOWN) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "media type enum not recognised, and hence ignored: '%s'", sections[1]);
			ret = FALSE;
			goto out;
		}

		ret = pk_backend_media_change_required (priv->backend, media_type_enum, sections[2], sections[3]);
		goto out;
	} else if (g_strcmp0 (command, "distro-upgrade") == 0) {

		if (size != 4) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}

		distro_upgrade_enum = pk_distro_upgrade_enum_from_string (sections[1]);
		if (distro_upgrade_enum == PK_DISTRO_UPGRADE_ENUM_UNKNOWN) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR,
					    "distro upgrade enum not recognised, and hence ignored: '%s'", sections[1]);
			ret = FALSE;
			goto out;
		}

		ret = pk_backend_distro_upgrade (priv->backend, distro_upgrade_enum, sections[2], sections[3]);
		goto out;
	} else if (g_strcmp0 (command, "category") == 0) {

		if (size != 6) {
			egg_warning ("invalid command'%s', size %i", command, size);
			ret = FALSE;
			goto out;
		}
		if (g_strcmp0 (sections[1], sections[2]) == 0) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR, "cat_id cannot be the same as parent_id");
			ret = FALSE;
			goto out;
		}
		if (egg_strzero (sections[2])) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR, "cat_id cannot not blank");
			ret = FALSE;
			goto out;
		}
		if (egg_strzero (sections[3])) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR, "name cannot not blank");
			ret = FALSE;
			goto out;
		}
		if (egg_strzero (sections[5])) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR, "icon cannot not blank");
			ret = FALSE;
			goto out;
		}
		if (g_str_has_prefix (sections[5], "/")) {
			pk_backend_message (priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR, "icon '%s' should be a named icon, not a path", sections[5]);
			ret = FALSE;
			goto out;
		}
		ret = pk_backend_category (priv->backend, sections[1], sections[2], sections[3], sections[4], sections[5]);
		goto out;
	} else {
		egg_warning ("invalid command '%s'", command);
	}
out:
	g_strfreev (sections);
	return ret;
}

/**
 * pk_backend_spawn_exit_cb:
 **/
static void
pk_backend_spawn_exit_cb (PkSpawn *spawn, PkSpawnExitType exit_enum, PkBackendSpawn *backend_spawn)
{
	gboolean ret;
	g_return_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn));

	/* if we force killed the process, set an error */
	if (exit_enum == PK_SPAWN_EXIT_TYPE_SIGKILL) {
		/* we just call this failed, and set an error */
		pk_backend_error_code (backend_spawn->priv->backend, PK_ERROR_ENUM_PROCESS_KILL,
				       "Process had to be killed to be cancelled");
	}

	if (exit_enum == PK_SPAWN_EXIT_TYPE_DISPATCHER_EXIT ||
	    exit_enum == PK_SPAWN_EXIT_TYPE_DISPATCHER_CHANGED) {
		egg_debug ("dispatcher exited, nothing to see here");
		return;
	}

	/* only emit if not finished */
	if (!backend_spawn->priv->finished) {
		egg_warning ("script exited without doing finished");
		ret = pk_backend_has_set_error_code (backend_spawn->priv->backend);
		if (!ret) {
			pk_backend_error_code (backend_spawn->priv->backend, PK_ERROR_ENUM_INTERNAL_ERROR,
					       "The backend exited unexpectedly. "
					       "This is a serious error as the spawned backend did not complete the pending transaction.");
		}
		pk_backend_finished (backend_spawn->priv->backend);
	}
}

/**
 * pk_backend_spawn_stdout_cb:
 **/
static void
pk_backend_spawn_stdout_cb (PkBackendSpawn *spawn, const gchar *line, PkBackendSpawn *backend_spawn)
{
	gboolean ret;
	g_return_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn));

	/* do we ignore with a filter func ? */
	if (backend_spawn->priv->stdout_func != NULL) {
		ret = backend_spawn->priv->stdout_func (backend_spawn->priv->backend, line);
		if (!ret)
			return;
	}

	ret = pk_backend_spawn_parse_stdout (backend_spawn, line);
	if (!ret)
		egg_debug ("failed to parse '%s'", line);
}

/**
 * pk_backend_spawn_stderr_cb:
 **/
static void
pk_backend_spawn_stderr_cb (PkBackendSpawn *spawn, const gchar *line, PkBackendSpawn *backend_spawn)
{
	gboolean ret;
	g_return_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn));

	/* do we ignore with a filter func ? */
	if (backend_spawn->priv->stderr_func != NULL) {
		ret = backend_spawn->priv->stderr_func (backend_spawn->priv->backend, line);
		if (!ret)
			return;
	}

	/* send warning up to session, this is never going to be pretty... */
	egg_warning ("STDERR: %s", line);
	pk_backend_message (backend_spawn->priv->backend, PK_MESSAGE_ENUM_BACKEND_ERROR, "%s", line);
}

/**
 * pk_backend_spawn_convert_uri:
 *
 * Our proxy variable is typically 'username:password@server:port'
 * but http_proxy expects 'http://username:password@server:port/'
 **/
static gchar *
pk_backend_spawn_convert_uri (const gchar *proxy)
{
	GString *string;
	string = g_string_new (proxy);

	/* if we didn't specify a prefix, add a default one */
	if (!g_str_has_prefix (proxy, "http://") &&
	    !g_str_has_prefix (proxy, "https://") &&
	    !g_str_has_prefix (proxy, "ftp://")) {
		g_string_prepend (string, "http://");
	}

	/* if we didn't specify a trailing slash, add one */
	if (!g_str_has_suffix (proxy, "/")) {
		g_string_append_c (string, '/');
	}

	return g_string_free (string, FALSE);
}

/**
 * pk_backend_spawn_get_envp:
 *
 * Return all the environment variables the script will need
 **/
static gchar **
pk_backend_spawn_get_envp (PkBackendSpawn *backend_spawn)
{
	gchar **envp;
	gchar *proxy_http;
	gchar *proxy_ftp;
	gchar *locale;
	gchar *line;
	gchar *uri;
	gchar *eulas;
	gchar *transaction_id = NULL;
	const gchar *value;
	guint i;
	GPtrArray *array;
	gboolean ret;
	PkHintEnum interactive;
	PkBackendSpawnPrivate *priv = backend_spawn->priv;

	array = g_ptr_array_new_with_free_func (g_free);

	/* don't do this for all backends as it's a performance penalty */
	if (FALSE) {
		/* transaction_id */
		g_object_get (priv->backend,
			      "transaction-id", &transaction_id,
			      NULL);
		line = g_strdup_printf ("%s=%s", "transaction_id", transaction_id);
		g_ptr_array_add (array, line);
	}

	/* accepted eulas */
	eulas = pk_backend_get_accepted_eula_string (priv->backend);
	if (eulas != NULL){
		line = g_strdup_printf ("%s=%s", "accepted_eulas", eulas);
		g_ptr_array_add (array, line);
	}

	/* http_proxy */
	proxy_http = pk_backend_get_proxy_http (priv->backend);
	if (!egg_strzero (proxy_http)) {
		uri = pk_backend_spawn_convert_uri (proxy_http);
		line = g_strdup_printf ("%s=%s", "http_proxy", uri);
		g_ptr_array_add (array, line);
		g_free (uri);
	}

	/* ftp_proxy */
	proxy_ftp = pk_backend_get_proxy_ftp (priv->backend);
	if (!egg_strzero (proxy_ftp)) {
		uri = pk_backend_spawn_convert_uri (proxy_ftp);
		line = g_strdup_printf ("%s=%s", "ftp_proxy", uri);
		g_ptr_array_add (array, line);
		g_free (uri);
	}

	/* LANG */
	locale = pk_backend_get_locale (priv->backend);
	if (!egg_strzero (locale)) {
		line = g_strdup_printf ("%s=%s", "LANG", locale);
		g_ptr_array_add (array, line);
	}

	/* ROOT */
	value = pk_backend_get_root (priv->backend);
	if (!egg_strzero (value)) {
		line = g_strdup_printf ("%s=%s", "ROOT", value);
		g_ptr_array_add (array, line);
	}

	/* NETWORK */
	ret = pk_backend_is_online (priv->backend);
	line = g_strdup_printf ("%s=%s", "NETWORK", ret ? "TRUE" : "FALSE");
	g_ptr_array_add (array, line);

	/* BACKGROUND */
	ret = pk_backend_use_background (priv->backend);
	line = g_strdup_printf ("%s=%s", "BACKGROUND", ret ? "TRUE" : "FALSE");
	g_ptr_array_add (array, line);

	/* INTERACTIVE */
	g_object_get (priv->backend,
		      "interactive", &interactive,
		      NULL);
	line = g_strdup_printf ("%s=%s", "INTERACTIVE", interactive ? "TRUE" : "FALSE");
	g_ptr_array_add (array, line);

	/* ensure the malicious user can't inject anthing from the session */
	for (i=0; i<array->len; i++) {
		line = g_ptr_array_index (array, i);
		g_strdelimit (line, "\\;{}[]()*?%\n\r\t", '_');
		egg_debug ("setting evp '%s'", line);
	}

	envp = pk_ptr_array_to_strv (array);
	g_ptr_array_unref (array);

	g_free (proxy_http);
	g_free (proxy_ftp);
	g_free (locale);
	g_free (eulas);
	g_free (transaction_id);
	return envp;
}

/**
 * pk_backend_spawn_va_list_to_argv:
 * @string_first: the first string
 * @args: any subsequant string's
 *
 * Form a composite string array of the va_list
 *
 * Return value: the string array, or %NULL if invalid
 **/
static gchar **
pk_backend_spawn_va_list_to_argv (const gchar *string_first, va_list *args)
{
	GPtrArray *ptr_array;
	gchar **array;
	gchar *value_temp;
	guint i;

	g_return_val_if_fail (args != NULL, NULL);
	g_return_val_if_fail (string_first != NULL, NULL);

	/* find how many elements we have in a temp array */
	ptr_array = g_ptr_array_new ();
	g_ptr_array_add (ptr_array, g_strdup (string_first));

	/* process all the va_list entries */
	for (i=0;; i++) {
		value_temp = va_arg (*args, gchar *);
		if (value_temp == NULL)
			break;
		g_ptr_array_add (ptr_array, g_strdup (value_temp));
	}

	/* convert the array to a strv type */
	array = pk_ptr_array_to_strv (ptr_array);

	/* get rid of the array, and free the contents */
	g_ptr_array_foreach (ptr_array, (GFunc) g_free, NULL);
	g_ptr_array_free (ptr_array, TRUE);
	return array;
}

/**
 * pk_backend_spawn_helper_va_list:
 **/
static gboolean
pk_backend_spawn_helper_va_list (PkBackendSpawn *backend_spawn, const gchar *executable, va_list *args)
{
	gboolean ret;
	gchar *filename;
	gchar **argv;
	gchar **envp;
	PkHintEnum background;
	PkBackendSpawnPrivate *priv = backend_spawn->priv;
#if PK_BUILD_LOCAL
	const gchar *directory;
#endif

	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), FALSE);

	/* convert to a argv */
	argv = pk_backend_spawn_va_list_to_argv (executable, args);
	if (argv == NULL) {
		egg_warning ("argv NULL");
		return FALSE;
	}

#if PK_BUILD_LOCAL
	/* prefer the local version */
	directory = priv->name;
	if (g_str_has_prefix (directory, "test_"))
		directory = "test";

	filename = g_build_filename ("..", "backends", directory, "helpers", argv[0], NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE) {
		egg_debug ("local helper not found '%s'", filename);
		g_free (filename);
		filename = g_build_filename ("..", "backends", directory, argv[0], NULL);
	}
	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE) {
		egg_debug ("local helper not found '%s'", filename);
		g_free (filename);
		filename = g_build_filename (DATADIR, "PackageKit", "helpers", priv->name, argv[0], NULL);
	}
#else
	filename = g_build_filename (DATADIR, "PackageKit", "helpers", priv->name, argv[0], NULL);
#endif
	egg_debug ("using spawn filename %s", filename);

	/* replace the filename with the full path */
	g_free (argv[0]);
	argv[0] = g_strdup (filename);

	/* copy idle setting from backend to PkSpawn instance */
	g_object_get (priv->backend,
		      "background", &background,
		      NULL);
	g_object_set (priv->spawn,
		      "background", (background == PK_HINT_ENUM_TRUE),
		      NULL);

	priv->finished = FALSE;
	envp = pk_backend_spawn_get_envp (backend_spawn);
	ret = pk_spawn_argv (priv->spawn, argv, envp);
	if (!ret) {
		pk_backend_error_code (priv->backend, PK_ERROR_ENUM_INTERNAL_ERROR,
				       "Spawn of helper '%s' failed", argv[0]);
		pk_backend_finished (priv->backend);
	}
	g_free (filename);
	g_strfreev (argv);
	g_strfreev (envp);
	return ret;
}

/**
 * pk_backend_spawn_get_name:
 **/
const gchar *
pk_backend_spawn_get_name (PkBackendSpawn *backend_spawn)
{
	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), NULL);
	return backend_spawn->priv->name;
}

/**
 * pk_backend_spawn_set_name:
 **/
gboolean
pk_backend_spawn_set_name (PkBackendSpawn *backend_spawn, const gchar *name)
{
	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	g_free (backend_spawn->priv->name);
	backend_spawn->priv->name = g_strdup (name);
	return TRUE;
}

/**
 * pk_backend_spawn_kill:
 *
 * A forceful exit, useful for aborting scripts
 **/
gboolean
pk_backend_spawn_kill (PkBackendSpawn *backend_spawn)
{
	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), FALSE);
	pk_spawn_kill (backend_spawn->priv->spawn);
	return TRUE;
}

/**
 * pk_backend_spawn_exit:
 *
 * A gentle nudge to an idle backend that it should be shut down
 **/
gboolean
pk_backend_spawn_exit (PkBackendSpawn *backend_spawn)
{
	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), FALSE);
	pk_spawn_exit (backend_spawn->priv->spawn);
	return TRUE;
}

/**
 * pk_backend_spawn_helper:
 **/
gboolean
pk_backend_spawn_helper (PkBackendSpawn *backend_spawn, const gchar *first_element, ...)
{
	gboolean ret;
	va_list args;

	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), FALSE);
	g_return_val_if_fail (first_element != NULL, FALSE);
	g_return_val_if_fail (backend_spawn->priv->name != NULL, FALSE);

	/* don't auto-kill this */
	if (backend_spawn->priv->kill_id > 0) {
		g_source_remove (backend_spawn->priv->kill_id);
		backend_spawn->priv->kill_id = 0;
	}

	/* get the argument list */
	va_start (args, first_element);
	ret = pk_backend_spawn_helper_va_list (backend_spawn, first_element, &args);
	va_end (args);

	return ret;
}

/**
 * pk_backend_spawn_set_allow_sigkill:
 **/
gboolean
pk_backend_spawn_set_allow_sigkill (PkBackendSpawn *backend_spawn, gboolean allow_sigkill)
{
	gboolean ret = TRUE;

	g_return_val_if_fail (PK_IS_BACKEND_SPAWN (backend_spawn), FALSE);

	/* have we banned this in the config ile */
	if (!backend_spawn->priv->allow_sigkill && allow_sigkill) {
		egg_warning ("cannot set allow_cancel TRUE as BackendSpawnAllowSIGKILL is set to FALSE in PackageKit.conf");
		ret = FALSE;
		goto out;
	}

	/* set this property */
	g_object_set (backend_spawn->priv->spawn,
		      "allow-sigkill", allow_sigkill,
		      NULL);
out:
	return ret;
}

/**
 * pk_backend_spawn_finalize:
 **/
static void
pk_backend_spawn_finalize (GObject *object)
{
	PkBackendSpawn *backend_spawn;

	g_return_if_fail (PK_IS_BACKEND_SPAWN (object));

	backend_spawn = PK_BACKEND_SPAWN (object);

	if (backend_spawn->priv->kill_id > 0)
		g_source_remove (backend_spawn->priv->kill_id);

	g_free (backend_spawn->priv->name);
	g_object_unref (backend_spawn->priv->conf);
	g_object_unref (backend_spawn->priv->spawn);
	g_object_unref (backend_spawn->priv->backend);

	G_OBJECT_CLASS (pk_backend_spawn_parent_class)->finalize (object);
}

/**
 * pk_backend_spawn_class_init:
 **/
static void
pk_backend_spawn_class_init (PkBackendSpawnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = pk_backend_spawn_finalize;
	g_type_class_add_private (klass, sizeof (PkBackendSpawnPrivate));
}

/**
 * pk_backend_spawn_init:
 **/
static void
pk_backend_spawn_init (PkBackendSpawn *backend_spawn)
{
	backend_spawn->priv = PK_BACKEND_SPAWN_GET_PRIVATE (backend_spawn);
	backend_spawn->priv->kill_id = 0;
	backend_spawn->priv->name = NULL;
	backend_spawn->priv->stdout_func = NULL;
	backend_spawn->priv->stderr_func = NULL;
	backend_spawn->priv->finished = FALSE;
	backend_spawn->priv->conf = pk_conf_new ();
	backend_spawn->priv->backend = pk_backend_new ();
	backend_spawn->priv->spawn = pk_spawn_new ();
	g_signal_connect (backend_spawn->priv->spawn, "exit",
			  G_CALLBACK (pk_backend_spawn_exit_cb), backend_spawn);
	g_signal_connect (backend_spawn->priv->spawn, "stdout",
			  G_CALLBACK (pk_backend_spawn_stdout_cb), backend_spawn);
	g_signal_connect (backend_spawn->priv->spawn, "stderr",
			  G_CALLBACK (pk_backend_spawn_stderr_cb), backend_spawn);

	/* set if SIGKILL is allowed */
	backend_spawn->priv->allow_sigkill = pk_conf_get_bool (backend_spawn->priv->conf, "BackendSpawnAllowSIGKILL");
	g_object_set (backend_spawn->priv->spawn,
		      "allow-sigkill", backend_spawn->priv->allow_sigkill,
		      NULL);
}

/**
 * pk_backend_spawn_new:
 **/
PkBackendSpawn *
pk_backend_spawn_new (void)
{
	PkBackendSpawn *backend_spawn;
	backend_spawn = g_object_new (PK_TYPE_BACKEND_SPAWN, NULL);
	return PK_BACKEND_SPAWN (backend_spawn);
}


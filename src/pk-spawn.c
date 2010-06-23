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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <sys/wait.h>
#include <fcntl.h>

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "egg-debug.h"
#include "egg-string.h"

#include "pk-spawn.h"
#include "pk-marshal.h"
#include "pk-conf.h"

#include "pk-sysdep.h"

static void     pk_spawn_finalize	(GObject       *object);

#define PK_SPAWN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PK_TYPE_SPAWN, PkSpawnPrivate))
#define PK_SPAWN_POLL_DELAY	50 /* ms */
#define PK_SPAWN_SIGKILL_DELAY	2500 /* ms */

struct PkSpawnPrivate
{
	pid_t			 child_pid;
	gint			 stdin_fd;
	GIOChannel		*io_channel_stdout;
	GIOChannel		*io_channel_stderr;
	guint			 kill_id;
	gboolean		 finished;
	gboolean		 background;
	gboolean		 is_sending_exit;
	gboolean		 is_changing_dispatcher;
	gboolean		 allow_sigkill;
	PkSpawnExitType		 exit;
	GString			*stdout_buf;
	GString			*stderr_buf;
	gchar			*last_argv0;
	gchar			**last_envp;
	PkConf			*conf;
	GMainLoop		*waiting_for_exit;
	guint			 watch_child_id;
};

enum {
	SIGNAL_EXIT,
	SIGNAL_STDOUT,
	SIGNAL_STDERR,
	SIGNAL_LAST
};

enum {
	PROP_0,
	PROP_BACKGROUND,
	PROP_ALLOW_SIGKILL,
	PROP_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (PkSpawn, pk_spawn, G_TYPE_OBJECT)

/**
 * pk_spawn_emit_whole_lines:
 **/
static gboolean
pk_spawn_emit_whole_lines (PkSpawn *spawn, GString *string)
{
	guint i;
	guint size;
	gchar **lines;
	guint bytes_processed;

	/* if nothing then don't emit */
	if (egg_strzero (string->str))
		return FALSE;

	/* split into lines - the last line may be incomplete */
	lines = g_strsplit (string->str, "\n", 0);
	if (lines == NULL)
		return FALSE;

	/* find size */
	size = g_strv_length (lines);

	bytes_processed = 0;
	/* we only emit n-1 strings */
	for (i=0; i<(size-1); i++) {
		g_signal_emit (spawn, signals [SIGNAL_STDOUT], 0, lines[i]);
		/* ITS4: ignore, g_strsplit always NULL terminates */
		bytes_processed += strlen (lines[i]) + 1;
	}

	/* remove the text we've processed */
	g_string_erase (string, 0, bytes_processed);

	g_strfreev (lines);
	return TRUE;
}

/**
 * pk_spawn_exit_type_enum_to_string:
 **/
static const gchar *
pk_spawn_exit_type_enum_to_string (PkSpawnExitType type)
{
	if (type == PK_SPAWN_EXIT_TYPE_SUCCESS)
		return "success";
	if (type == PK_SPAWN_EXIT_TYPE_FAILED)
		return "failed";
	if (type == PK_SPAWN_EXIT_TYPE_DISPATCHER_CHANGED)
		return "dispatcher-changed";
	if (type == PK_SPAWN_EXIT_TYPE_DISPATCHER_EXIT)
		return "dispatcher-exit";
	if (type == PK_SPAWN_EXIT_TYPE_SIGQUIT)
		return "sigquit";
	if (type == PK_SPAWN_EXIT_TYPE_SIGKILL)
		return "sigkill";
	return "unknown";
}

/**
 * pk_spawn_sigkill_cb:
 **/
static gboolean
pk_spawn_sigkill_cb (PkSpawn *spawn)
{
	gint retval;

	/* check if process has already gone */
	if (spawn->priv->finished) {
		egg_warning ("already finished, ignoring");
		return FALSE;
	}

	/* set this in case the script catches the signal and exits properly */
	spawn->priv->exit = PK_SPAWN_EXIT_TYPE_SIGKILL;

	egg_debug ("sending SIGKILL %ld", (long)spawn->priv->child_pid);
	retval = kill (spawn->priv->child_pid, SIGKILL);
	if (retval == EINVAL) {
		egg_warning ("The signum argument is an invalid or unsupported number");
		return FALSE;
	} else if (retval == EPERM) {
		egg_warning ("You do not have the privilege to send a signal to the process");
		return FALSE;
	}

	/* never repeat */
	return FALSE;
}

/**
 * pk_spawn_is_running:
 *
 * Is this instance controlling a script?
 *
 **/
gboolean
pk_spawn_is_running (PkSpawn *spawn)
{
	return (spawn->priv->child_pid != -1);
}

/**
 * pk_spawn_kill:
 *
 * We send SIGQUIT and after a few ms SIGKILL (if allowed)
 *
 * IMPORTANT: This is not a syncronous operation, and client programs will need
 * to wait for the ::exit signal.
 **/
gboolean
pk_spawn_kill (PkSpawn *spawn)
{
	gint retval;

	g_return_val_if_fail (PK_IS_SPAWN (spawn), FALSE);
	g_return_val_if_fail (spawn->priv->kill_id == 0, FALSE);

	/* is there a process running? */
	if (spawn->priv->child_pid == -1) {
		egg_warning ("no child pid to kill!");
		return FALSE;
	}

	/* check if process has already gone */
	if (spawn->priv->finished) {
		egg_warning ("already finished, ignoring");
		return FALSE;
	}

	/* set this in case the script catches the signal and exits properly */
	spawn->priv->exit = PK_SPAWN_EXIT_TYPE_SIGQUIT;

	egg_debug ("sending SIGQUIT %ld", (long)spawn->priv->child_pid);
	retval = kill (spawn->priv->child_pid, SIGQUIT);
	if (retval == EINVAL) {
		egg_warning ("The signum argument is an invalid or unsupported number");
		return FALSE;
	} else if (retval == EPERM) {
		egg_warning ("You do not have the privilege to send a signal to the process");
		return FALSE;
	}

	/* the program might not be able to handle SIGQUIT, give it a few seconds and then SIGKILL it */
	if (spawn->priv->allow_sigkill) {
		spawn->priv->kill_id = g_timeout_add (PK_SPAWN_SIGKILL_DELAY, (GSourceFunc) pk_spawn_sigkill_cb, spawn);
#if GLIB_CHECK_VERSION(2,25,8)
		g_source_set_name_by_id (spawn->priv->kill_id, "[PkSpawn] sigkill");
#endif
	}
	return TRUE;
}

/**
 * pk_spawn_send_stdin:
 *
 * Send new comands to a running (but idle) dispatcher script
 *
 **/
static gboolean
pk_spawn_send_stdin (PkSpawn *spawn, const gchar *command)
{
	gint wrote;
	gint length;
	gchar *buffer = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (PK_IS_SPAWN (spawn), FALSE);

	/* check if process has already gone */
	if (spawn->priv->finished) {
		egg_warning ("already finished, ignoring");
		ret = FALSE;
		goto out;
	}

	/* is there a process running? */
	if (spawn->priv->child_pid == -1) {
		egg_debug ("no child pid");
		ret = FALSE;
		goto out;
	}

	/* buffer always has to have trailing newline */
	egg_debug ("sending '%s'", command);
	buffer = g_strdup_printf ("%s\n", command);

	/* ITS4: ignore, we generated this */
	length = strlen (buffer);

	/* write to the waiting process */
	wrote = write (spawn->priv->stdin_fd, buffer, length);
	if (wrote != length) {
		egg_warning ("wrote %i/%i bytes on fd %i (%s)", wrote, length, spawn->priv->stdin_fd, strerror (errno));
		ret = FALSE;
	}
out:
	g_free (buffer);
	return ret;
}

/**
 * pk_spawn_exit:
 *
 * Just write "exit" into the open fd and block until the backend is closed
 *
 **/
gboolean
pk_spawn_exit (PkSpawn *spawn)
{
	gboolean ret;

	g_return_val_if_fail (PK_IS_SPAWN (spawn), FALSE);

	/* check if already sending exit */
	if (spawn->priv->is_sending_exit) {
		egg_warning ("already sending exit, ignoring");
		return FALSE;
	}

	/* send command */
	spawn->priv->is_sending_exit = TRUE;
	spawn->priv->exit = PK_SPAWN_EXIT_TYPE_DISPATCHER_EXIT;
	ret = pk_spawn_send_stdin (spawn, "exit");
	if (!ret) {
		egg_warning ("failed to send exit");
		goto out;
	}

	/* block until the previous script exited */
	g_main_loop_run (spawn->priv->waiting_for_exit);
	if (spawn->priv->exit != PK_SPAWN_EXIT_TYPE_DISPATCHER_EXIT)
		egg_warning ("failed to exit script");
out:
	spawn->priv->is_sending_exit = FALSE;
	return ret;
}

/**
 * pk_spawn_child_watch_cb:
 **/
static gboolean
pk_spawn_child_watch_cb (GPid pid, gint status, PkSpawn *spawn)
{
	gint retval;

	/* this shouldn't happen */
	if (spawn->priv->finished) {
		egg_warning ("finished twice!");
		return FALSE;
	}

	/* child exited, close resources */
	close (spawn->priv->stdin_fd);
	spawn->priv->stdin_fd = -1;
	spawn->priv->child_pid = -1;
	spawn->priv->watch_child_id = 0;
	g_io_channel_unref (spawn->priv->io_channel_stdout);
	g_io_channel_unref (spawn->priv->io_channel_stderr);

	/* use this to detect SIGKILL and SIGQUIT */
	if (WIFSIGNALED (status)) {
		retval = WTERMSIG (status);
		if (retval == SIGQUIT) {
			egg_debug ("the child process was terminated by SIGQUIT");
			spawn->priv->exit = PK_SPAWN_EXIT_TYPE_SIGQUIT;
		} else if (retval == SIGKILL) {
			egg_debug ("the child process was terminated by SIGKILL");
			spawn->priv->exit = PK_SPAWN_EXIT_TYPE_SIGKILL;
		} else {
			egg_warning ("the child process was terminated by signal %i", WTERMSIG (status));
			spawn->priv->exit = PK_SPAWN_EXIT_TYPE_SIGKILL;
		}
	} else {
		/* check we are dead and buried */
		if (!WIFEXITED (status)) {
			egg_error ("the process did not exit, but waitpid() returned!");
			return TRUE;
		}

		/* get the exit code */
		retval = WEXITSTATUS (status);
		if (retval == 0) {
			egg_debug ("the child exited with success");
			if (spawn->priv->exit == PK_SPAWN_EXIT_TYPE_UNKNOWN)
				spawn->priv->exit = PK_SPAWN_EXIT_TYPE_SUCCESS;
		} else {
			egg_warning ("the child exited with return code %i", retval);
			if (spawn->priv->exit == PK_SPAWN_EXIT_TYPE_UNKNOWN)
				spawn->priv->exit = PK_SPAWN_EXIT_TYPE_FAILED;
		}
	}

	/* officially done, although no signal yet */
	spawn->priv->finished = TRUE;

	/* if we are trying to kill this process, cancel the SIGKILL */
	if (spawn->priv->kill_id != 0) {
		g_source_remove (spawn->priv->kill_id);
		spawn->priv->kill_id = 0;
	}

	/* are we doing pk_spawn_exit for a good reason? */
	if (spawn->priv->is_changing_dispatcher)
		spawn->priv->exit = PK_SPAWN_EXIT_TYPE_DISPATCHER_CHANGED;
	else if (spawn->priv->is_sending_exit)
		spawn->priv->exit = PK_SPAWN_EXIT_TYPE_DISPATCHER_EXIT;

	/* don't emit if we just closed an invalid dispatcher */
	egg_debug ("emitting exit %s", pk_spawn_exit_type_enum_to_string (spawn->priv->exit));
	g_signal_emit (spawn, signals [SIGNAL_EXIT], 0, spawn->priv->exit);

	/* are we waiting internally */
	if (g_main_loop_is_running (spawn->priv->waiting_for_exit))
		g_main_loop_quit (spawn->priv->waiting_for_exit);

	return TRUE;
}

/**
 * pk_spawn_io_channel_stdout_handler_cb:
 **/
static gboolean
pk_spawn_io_channel_stdout_handler_cb (GIOChannel *source, GIOCondition condition, PkSpawn *spawn)
{
	gchar *line = NULL;
	GError *error = NULL;
	GIOStatus status;

	/* read data */
	status = g_io_channel_read_line_string (source, spawn->priv->stdout_buf, NULL, &error);
	if (status != G_IO_STATUS_NORMAL) {
		egg_warning ("failed to read: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* all usual output goes on standard out, only bad libraries bitch to stderr */
	pk_spawn_emit_whole_lines (spawn, spawn->priv->stdout_buf);
out:
	g_free (line);
	return TRUE;
}

/**
 * pk_spawn_io_channel_stderr_handler_cb:
 **/
static gboolean
pk_spawn_io_channel_stderr_handler_cb (GIOChannel *source, GIOCondition condition, PkSpawn *spawn)
{
	gchar *line = NULL;
	GError *error = NULL;
	GIOStatus status;

	/* read data */
	status = g_io_channel_read_line (source, &line, NULL, NULL, &error);
	if (status != G_IO_STATUS_NORMAL) {
		egg_warning ("failed to read: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* emit all lines on standard out in one callback, as it's all probably
	* related to the error that just happened */
	g_signal_emit (spawn, signals [SIGNAL_STDERR], 0, line);
out:
	g_free (line);
	return TRUE;
}

/**
 * pk_spawn_argv:
 * @argv: Can be generated using g_strsplit (command, " ", 0)
 * if there are no spaces in the filename
 *
 **/
gboolean
pk_spawn_argv (PkSpawn *spawn, gchar **argv, gchar **envp)
{
	gboolean ret;
	gboolean idleio;
	guint i;
	guint len;
	gint nice_value;
	gchar *command;
	const gchar *key;
	gint stderr_fd = -1;
	gint stdout_fd = -1;

	g_return_val_if_fail (PK_IS_SPAWN (spawn), FALSE);
	g_return_val_if_fail (argv != NULL, FALSE);

	len = g_strv_length (argv);
	for (i=0; i<len; i++)
		egg_debug ("argv[%i] '%s'", i, argv[i]);
	if (envp != NULL) {
		len = g_strv_length (envp);
		for (i=0; i<len; i++)
			egg_debug ("envp[%i] '%s'", i, envp[i]);
	}

	/* check we are not using a closing instance */
	if (spawn->priv->is_sending_exit) {
		egg_warning ("trying to use instance that is in the process of exiting");
		return FALSE;
	}

	/* we can reuse the dispatcher if:
	 *  - it's still running
	 *  - argv[0] (executable name is the same)
	 *  - all of envp are the same (proxy and locale settings) */
	if (spawn->priv->stdin_fd != -1) {
		if (g_strcmp0 (spawn->priv->last_argv0, argv[0]) != 0) {
			egg_debug ("argv did not match, not reusing");
		} else if (!egg_strvequal (spawn->priv->last_envp, envp)) {
			egg_debug ("envp did not match, not reusing");
		} else {
			/* join with tabs, as spaces could be in file name */
			command = g_strjoinv ("\t", &argv[1]);

			/* reuse instance */
			egg_debug ("reusing instance");
			ret = pk_spawn_send_stdin (spawn, command);
			g_free (command);
			if (ret)
				return TRUE;

			/* so fall on through to kill and respawn */
			egg_warning ("failed to write, so trying to kill and respawn");
		}

		/* kill off existing instance */
		egg_debug ("changing dispatcher (exit old instance)");
		spawn->priv->is_changing_dispatcher = TRUE;
		ret = pk_spawn_exit (spawn);
		if (!ret) {
			egg_warning ("failed to exit previous instance");
		}
		spawn->priv->is_changing_dispatcher = FALSE;
	}

	/* create spawned object for tracking */
	spawn->priv->finished = FALSE;
	egg_debug ("creating new instance of %s", argv[0]);
	ret = g_spawn_async_with_pipes (NULL, argv, envp,
				 G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
				 NULL, NULL, &spawn->priv->child_pid,
				 &spawn->priv->stdin_fd,
				 &stdout_fd,
				 &stderr_fd,
				 NULL);
	if (!ret) {
		egg_warning ("failed to spawn '%s'", argv[0]);
		return FALSE;
	}

	/* watch this process */
	spawn->priv->io_channel_stdout = g_io_channel_unix_new (stdout_fd);
	spawn->priv->io_channel_stderr = g_io_channel_unix_new (stderr_fd);
	g_io_add_watch (spawn->priv->io_channel_stdout, G_IO_IN, (GIOFunc) pk_spawn_io_channel_stdout_handler_cb, spawn);
	g_io_add_watch (spawn->priv->io_channel_stderr, G_IO_IN, (GIOFunc) pk_spawn_io_channel_stderr_handler_cb, spawn);
	spawn->priv->watch_child_id = g_child_watch_add (spawn->priv->child_pid, (GChildWatchFunc) pk_spawn_child_watch_cb, spawn);

	/* get the nice value and ensure we are in the valid range */
	key = "BackendSpawnNiceValue";
	if (spawn->priv->background)
		key = "BackendSpawnNiceValueBackground";
	nice_value = pk_conf_get_int (spawn->priv->conf, key);
	nice_value = CLAMP(nice_value, -20, 19);

#if HAVE_SETPRIORITY
	/* don't completely bog the system down */
	if (nice_value != 0) {
		egg_debug ("renice to %i", nice_value);
		setpriority (PRIO_PROCESS, spawn->priv->child_pid, nice_value);
	}
#endif

	/* perhaps set idle IO priority */
	key = "BackendSpawnIdleIO";
	if (spawn->priv->background)
		key = "BackendSpawnIdleIOBackground";
	idleio = pk_conf_get_bool (spawn->priv->conf, key);
	if (idleio) {
		egg_debug ("setting ioprio class to idle");
		pk_ioprio_set_idle (spawn->priv->child_pid);
	}

	/* save this so we can check the dispatcher name */
	g_free (spawn->priv->last_argv0);
	spawn->priv->last_argv0 = g_strdup (argv[0]);

	/* save this in case the proxy or locale changes */
	g_strfreev (spawn->priv->last_envp);
	spawn->priv->last_envp = g_strdupv (envp);

	return TRUE;
}

/**
 * pk_spawn_get_property:
 **/
static void
pk_spawn_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	PkSpawn *spawn = PK_SPAWN (object);
	PkSpawnPrivate *priv = spawn->priv;

	switch (prop_id) {
	case PROP_BACKGROUND:
		g_value_set_boolean (value, priv->background);
		break;
	case PROP_ALLOW_SIGKILL:
		g_value_set_boolean (value, priv->allow_sigkill);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * pk_spawn_set_property:
 **/
static void
pk_spawn_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	PkSpawn *spawn = PK_SPAWN (object);
	PkSpawnPrivate *priv = spawn->priv;

	switch (prop_id) {
	case PROP_BACKGROUND:
		priv->background = g_value_get_boolean (value);
		break;
	case PROP_ALLOW_SIGKILL:
		priv->allow_sigkill = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * pk_spawn_class_init:
 * @klass: The PkSpawnClass
 **/
static void
pk_spawn_class_init (PkSpawnClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = pk_spawn_finalize;
	object_class->get_property = pk_spawn_get_property;
	object_class->set_property = pk_spawn_set_property;

	/**
	 * PkSpawn:background:
	 */
	pspec = g_param_spec_boolean ("background", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BACKGROUND, pspec);

	/**
	 * PkSpawn:allow-sigkill:
	 * Set whether the spawned backends are allowed to be SIGKILLed if they do not
	 * respond to SIGQUIT. This ensures that Cancel() works as expected, but
	 * sometimes can corrupt databases if they are open.
	 */
	pspec = g_param_spec_boolean ("allow-sigkill", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ALLOW_SIGKILL, pspec);

	signals [SIGNAL_EXIT] =
		g_signal_new ("exit",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals [SIGNAL_STDOUT] =
		g_signal_new ("stdout",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals [SIGNAL_STDERR] =
		g_signal_new ("stderr",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (PkSpawnPrivate));
}

/**
 * pk_spawn_init:
 * @spawn: This class instance
 **/
static void
pk_spawn_init (PkSpawn *spawn)
{
	spawn->priv = PK_SPAWN_GET_PRIVATE (spawn);

	spawn->priv->child_pid = -1;
	spawn->priv->io_channel_stdout = NULL;
	spawn->priv->io_channel_stdout = NULL;
	spawn->priv->stdin_fd = -1;
	spawn->priv->kill_id = 0;
	spawn->priv->watch_child_id = 0;
	spawn->priv->finished = FALSE;
	spawn->priv->is_sending_exit = FALSE;
	spawn->priv->is_changing_dispatcher = FALSE;
	spawn->priv->allow_sigkill = TRUE;
	spawn->priv->last_argv0 = NULL;
	spawn->priv->last_envp = NULL;
	spawn->priv->background = FALSE;
	spawn->priv->exit = PK_SPAWN_EXIT_TYPE_UNKNOWN;

	spawn->priv->stdout_buf = g_string_new ("");
	spawn->priv->stderr_buf = g_string_new ("");
	spawn->priv->conf = pk_conf_new ();
	spawn->priv->waiting_for_exit = g_main_loop_new (NULL, FALSE);
}

/**
 * pk_spawn_finalize:
 * @object: The object to finalize
 **/
static void
pk_spawn_finalize (GObject *object)
{
	PkSpawn *spawn;

	g_return_if_fail (object != NULL);
	g_return_if_fail (PK_IS_SPAWN (object));

	spawn = PK_SPAWN (object);

	g_return_if_fail (spawn->priv != NULL);

	/* disconnect the SIGKILL check */
	if (spawn->priv->kill_id != 0) {
		g_source_remove (spawn->priv->kill_id);
		spawn->priv->kill_id = 0;
	}

	/* still running? */
	if (spawn->priv->stdin_fd != -1) {
		egg_warning ("killing as still running");
		pk_spawn_kill (spawn);
		/* just hope the script responded to SIGQUIT */
		if (spawn->priv->kill_id != 0)
			g_source_remove (spawn->priv->kill_id);
	}

	if (spawn->priv->watch_child_id > 0)
		g_source_remove (spawn->priv->watch_child_id);

	/* free the buffers */
	g_string_free (spawn->priv->stdout_buf, TRUE);
	g_string_free (spawn->priv->stderr_buf, TRUE);
	g_free (spawn->priv->last_argv0);
	g_strfreev (spawn->priv->last_envp);
	g_object_unref (spawn->priv->conf);
	g_main_loop_unref (spawn->priv->waiting_for_exit);

	G_OBJECT_CLASS (pk_spawn_parent_class)->finalize (object);
}

/**
 * pk_spawn_new:
 *
 * Return value: a new PkSpawn object.
 **/
PkSpawn *
pk_spawn_new (void)
{
	PkSpawn *spawn;
	spawn = g_object_new (PK_TYPE_SPAWN, NULL);
	return PK_SPAWN (spawn);
}


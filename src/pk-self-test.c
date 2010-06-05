/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include "pk-backend-internal.h"
#include "pk-cache.h"
#include "pk-conf.h"
#include "pk-lsof.h"
#include "pk-time.h"
#include "pk-transaction-db.h"
#include "pk-transaction.h"
#include "pk-transaction-extra.h"
#include "pk-transaction-list.h"
#include "pk-syslog.h"
#include "pk-dbus.h"
#include "pk-inhibit.h"
#include "pk-proc.h"
#include "pk-file-monitor.h"
#include "pk-transaction-db.h"

#if 0

static guint number_messages = 0;
static guint number_packages = 0;

/**
 * pk_test_backend_message_cb:
 **/
static void
pk_test_backend_message_cb (PkBackend *backend, PkMessageEnum message, const gchar *details, gpointer data)
{
	egg_debug ("details=%s", details);
	number_messages++;
}

/**
 * pk_test_backend_finished_cb:
 **/
static void
pk_test_backend_finished_cb (PkBackend *backend, PkExitEnum exit, GMainLoop *loop)
{
	g_main_loop_quit (loop)
}

/**
 * pk_test_backend_watch_file_cb:
 **/
static void
pk_test_backend_watch_file_cb (PkBackend *backend, GMainLoop *loop)
{
	g_main_loop_quit (loop)
}

static gboolean
pk_test_backend_func_true (PkBackend *backend)
{
	g_usleep (1000*1000);
	/* trigger duplicate test */
	pk_backend_package (backend, PK_INFO_ENUM_AVAILABLE, "vips-doc;7.12.4-2.fc8;noarch;linva", "The vips documentation package.");
	pk_backend_package (backend, PK_INFO_ENUM_AVAILABLE, "vips-doc;7.12.4-2.fc8;noarch;linva", "The vips documentation package.");
	pk_backend_finished (backend);
	return TRUE;
}

static gboolean
pk_test_backend_func_immediate_false (PkBackend *backend)
{
	pk_backend_finished (backend);
	return FALSE;
}

/**
 * pk_test_backend_package_cb:
 **/
static void
pk_test_backend_package_cb (PkBackend *backend, PkPackage *item, GMainLoop *loop)
{
	egg_debug ("package:%s", pk_package_get_id (item));
	number_packages++;
}

static void
pk_test_backend_func (void)
{
	PkBackend *backend;
	PkConf *conf;
	gchar *text;
	gchar *text_safe;
	gboolean ret;
	const gchar *filename;
	gboolean developer_mode;

	/* test replace unsafe (okay) */
	text_safe = pk_backend_strsafe ("Richard Hughes");
	g_assert_cmpstr (text_safe, ==, "Richard Hughes");
	g_free (text_safe);

	/* test replace UTF8 unsafe (okay) */
	text_safe = pk_backend_strsafe ("Gölas");
	g_assert_cmpstr (text_safe, ==, "Gölas");
	g_free (text_safe);

	/* test replace unsafe (one invalid) */
	text_safe = pk_backend_strsafe ("Richard\rHughes");
	g_assert_cmpstr (text_safe, ==, "Richard Hughes");
	g_free (text_safe);

	/* test replace unsafe (multiple invalid) */
	text_safe = pk_backend_strsafe (" Richard\rHughes\f");
	g_assert_cmpstr (text_safe, ==, " Richard Hughes ");
	g_free (text_safe);

	/* get an backend */
	backend = pk_backend_new ();
	if (backend != NULL);

	/* connect */
	g_signal_connect (backend, "package",
			  G_CALLBACK (pk_test_backend_package_cb), test);

	/* create a config file */
	filename = "/tmp/dave";
	ret = g_file_set_contents (filename, "foo", -1, NULL);
	g_assert (ret);

	/* set up a watch file on a config file */
	ret = pk_backend_watch_file (backend, filename, pk_backend_test_watch_file_cb, test);
	g_assert (ret);

	/* change the config file */
	ret = g_file_set_contents (filename, "bar", -1, NULL);
	g_assert (ret);

	/* wait for config file change */
	egg_test_loop_wait (test, 2000);
	egg_test_loop_check ();

	/* delete the config file */
	ret = g_unlink (filename);
	g_assert (!ret);

	g_signal_connect (backend, "message", G_CALLBACK (pk_test_backend_message_cb), NULL);
	g_signal_connect (backend, "finished", G_CALLBACK (pk_test_backend_finished_cb), test);

	/* get eula that does not exist */
	ret = pk_backend_is_eula_valid (backend, "license_foo");
	g_assert (!ret);

	/* accept eula */
	ret = pk_backend_accept_eula (backend, "license_foo");
	g_assert (ret);

	/* get eula that does exist */
	ret = pk_backend_is_eula_valid (backend, "license_foo");
	g_assert (ret);

	/* accept eula (again) */
	ret = pk_backend_accept_eula (backend, "license_foo");
	g_assert (!ret);

	/* load an invalid backend */
	ret = pk_backend_set_name (backend, "invalid");
	g_assert (!ret);

	/* try to load a valid backend */
	ret = pk_backend_set_name (backend, "dummy");
	g_assert (ret);

	/* load an valid backend again */
	ret = pk_backend_set_name (backend, "dummy");
	g_assert (!ret);

	/* lock an valid backend */
	ret = pk_backend_lock (backend);
	g_assert (ret);

	/* lock a backend again */
	ret = pk_backend_lock (backend);
	g_assert (ret);

	/* check we are out of init */
	g_assert (!backend->priv->during_initialize);

	/* get backend name */
	text = pk_backend_get_name (backend);
	g_assert_cmpstr (text, ==, "dummy");
	g_free (text);

	/* unlock an valid backend */
	ret = pk_backend_unlock (backend);
	g_assert (ret);

	/* unlock an valid backend again */
	ret = pk_backend_unlock (backend);
	g_assert (ret);

	/* check we are not finished */
	g_assert (!backend->priv->finished);

	/* check we have no error */
	g_assert (!backend->priv->set_error);

	/* lock again */
	ret = pk_backend_lock (backend);
	g_assert (ret);

	/* wait for a thread to return true */
	ret = pk_backend_thread_create (backend, pk_test_backend_func_true);
	g_assert (ret);

	/* wait for Finished */
	egg_test_loop_wait (test, 2000);
	egg_test_loop_check ();

	/* check duplicate filter */
	g_assert_cmpint (number_packages, ==, 1);

	/* reset */
	pk_backend_reset (backend);

	/* wait for a thread to return false (straight away) */
	ret = pk_backend_thread_create (backend, pk_test_backend_func_immediate_false);
	g_assert (ret);
	/* wait for Finished */
	egg_test_loop_wait (test, PK_BACKEND_FINISHED_TIMEOUT_GRACE + 100);
	egg_test_loop_check ();

	pk_backend_reset (backend);
	pk_backend_error_code (backend, PK_ERROR_ENUM_GPG_FAILURE, "test error");

	/* wait for finished */
	egg_test_loop_wait (test, PK_BACKEND_FINISHED_ERROR_TIMEOUT + 400);
	egg_test_loop_check ();

	/* get allow cancel after reset */
	pk_backend_reset (backend);
	ret = pk_backend_get_allow_cancel (backend);
	g_assert (!ret);

	/* set allow cancel TRUE */
	ret = pk_backend_set_allow_cancel (backend, TRUE);
	g_assert (ret);

	/* set allow cancel TRUE (repeat) */
	ret = pk_backend_set_allow_cancel (backend, TRUE);
	g_assert (!ret);

	/* set allow cancel FALSE */
	ret = pk_backend_set_allow_cancel (backend, FALSE);
	g_assert (ret);

	/* set allow cancel FALSE (after reset) */
	pk_backend_reset (backend);
	ret = pk_backend_set_allow_cancel (backend, FALSE);
	g_assert (ret);

	/* if running in developer mode, then expect a Message */
	conf = pk_conf_new ();
	developer_mode = pk_conf_get_bool (conf, "DeveloperMode");
	g_object_unref (conf);
	if (developer_mode) {
		/* check we enforce finished after error_code */
		g_assert_cmpint (number_messages, ==, 1)
	}

	g_object_unref (backend);
}

static GMainLoop *loop;
static guint number_packages = 0;

/**
 * pk_test_backend_spawn_test_finished_cb:
 **/
static void
pk_test_backend_spawn_test_finished_cb (PkBackend *backend, PkExitEnum exit, PkBackendSpawn *backend_spawn)
{
	g_main_loop_quit (loop);
}

/**
 * pk_test_backend_spawn_test_package_cb:
 **/
static void
pk_test_backend_spawn_test_package_cb (PkBackend *backend, PkInfoEnum info,
				  const gchar *package_id, const gchar *summary,
				  PkBackendSpawn *backend_spawn)
{
	number_packages++;
}

static gchar **
pk_test_backend_spawn_va_list_to_argv_test (const gchar *first_element, ...)
{
	va_list args;
	gchar **array;

	/* get the argument list */
	va_start (args, first_element);
	array = pk_backend_spawn_va_list_to_argv (first_element, &args);
	va_end (args);

	return array;
}

static void
pk_test_backend_spawn_func (void)
{
	PkBackendSpawn *backend_spawn;
	PkBackend *backend;
	const gchar *text;
	guint refcount;
	gboolean ret;
	gchar *uri;
	gchar **array;

	loop = g_main_loop_new (NULL, FALSE);

	/* va_list_to_argv single */
	array = pk_test_backend_spawn_va_list_to_argv_test ("richard", NULL);
	g_assert_cmpstr (array[0], ==, "richard");
	g_assert_cmpstr (array[1], ==, NULL);
	g_strfreev (array);

	/* va_list_to_argv triple */
	array = pk_test_backend_spawn_va_list_to_argv_test ("richard", "phillip", "hughes", NULL);
	g_assert_cmpstr (array[0], ==, "richard");
	g_assert_cmpstr (array[1], ==, "phillip");
	g_assert_cmpstr (array[2], ==, "hughes");
	g_assert_cmpstr (array[3], ==, NULL);
	g_strfreev (array);

	/* get an backend_spawn */
	backend_spawn = pk_backend_spawn_new ();
	g_assert (backend_spawn != NULL);

	/* private copy for unref testing */
	backend = backend_spawn->priv->backend;
	/* incr ref count so we don't kill the object */
	g_object_ref (backend);

	/* get backend name */
	text = pk_backend_spawn_get_name (backend_spawn);
	g_assert_cmpstr (text, ==, NULL);

	/* set backend name */
	ret = pk_backend_spawn_set_name (backend_spawn, "test_spawn");
	g_assert (ret);

	/* get backend name */
	text = pk_backend_spawn_get_name (backend_spawn);
	g_assert_cmpstr (text, ==, "test_spawn");

	/* needed to avoid an error */
	ret = pk_backend_set_name (backend_spawn->priv->backend, "test_spawn");
	g_assert (ret);
	ret = pk_backend_lock (backend_spawn->priv->backend);
	g_assert (ret);

	/* test pk_backend_spawn_parse_stdout Percentage1 */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "percentage\t0");
	g_assert (ret);

	/* test pk_backend_spawn_parse_stdout Percentage2 */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "percentage\tbrian");
	g_assert (!ret);

	/* test pk_backend_spawn_parse_stdout Percentage3 */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "percentage\t12345");
	g_assert (!ret);

	/* test pk_backend_spawn_parse_stdout Percentage4 */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "percentage\t");
	g_assert (!ret);

	/* test pk_backend_spawn_parse_stdout Percentage5 */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "percentage");
	g_assert (!ret);

	/* test pk_backend_spawn_parse_stdout Subpercentage */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "subpercentage\t17");
	g_assert (ret);

	/* test pk_backend_spawn_parse_stdout NoPercentageUpdates");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "no-percentage-updates");
	g_assert (ret);

	/* test pk_backend_spawn_parse_stdout failure */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "error\tnot-present-woohoo\tdescription text");
	g_assert (!ret);

	/* test pk_backend_spawn_parse_stdout Status */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "status\tquery");
	g_assert (ret);

	/* test pk_backend_spawn_parse_stdout RequireRestart */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "requirerestart\tsystem\tgnome-power-manager;0.0.1;i386;data");
	g_assert (ret);

	/* test pk_backend_spawn_parse_stdout RequireRestart invalid enum */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "requirerestart\tmooville\tgnome-power-manager;0.0.1;i386;data");
	g_assert (!ret);

	/* test pk_backend_spawn_parse_stdout RequireRestart invalid PackageId */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "requirerestart\tsystem\tdetails about the restart");
	g_assert (!ret);

	/* test pk_backend_spawn_parse_stdout AllowUpdate1 */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "allow-cancel\ttrue");
	g_assert (ret);

	/* test pk_backend_spawn_parse_stdout AllowUpdate2 */
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "allow-cancel\tbrian");
	g_assert (!ret);

	/* convert proxy uri (bare) */
	uri = pk_backend_spawn_convert_uri ("username:password@server:port");
	g_assert_cmpstr (uri, ==, "http://username:password@server:port/"));
	g_free (uri);

	/* convert proxy uri (full) */
	uri = pk_backend_spawn_convert_uri ("http://username:password@server:port/");
	g_assert_cmpstr (uri, ==, "http://username:password@server:port/"));
	g_free (uri);

	/* convert proxy uri (partial) */
	uri = pk_backend_spawn_convert_uri ("ftp://username:password@server:port");
	g_assert_cmpstr (uri, ==, "ftp://username:password@server:port/"));
	g_free (uri);

	/* test pk_backend_spawn_parse_common_out Package */
	ret = pk_backend_spawn_parse_stdout (backend_spawn,
		"package\tinstalled\tgnome-power-manager;0.0.1;i386;data\tMore useless software");
	g_assert (ret);

	/* manually unlock as we have no engine */
	ret = pk_backend_unlock (backend_spawn->priv->backend);
	g_assert (ret);

	/* reset */
	g_object_unref (backend_spawn);

	/* test we unref'd all but one of the PkBackend instances */
	refcount = G_OBJECT(backend)->ref_count;
	g_assert_cmpint (refcount, ==, 1);

	/* new */
	backend_spawn = pk_backend_spawn_new ();

	/* set backend name */
	ret = pk_backend_spawn_set_name (backend_spawn, "test_spawn");
	g_assert (ret);

	/* so we can spin until we finish */
	g_signal_connect (backend_spawn->priv->backend, "finished",
			  G_CALLBACK (pk_backend_spawn_test_finished_cb), backend_spawn);
	/* so we can count the returned packages */
	g_signal_connect (backend_spawn->priv->backend, "package",
			  G_CALLBACK (pk_backend_spawn_test_package_cb), backend_spawn);

	/* needed to avoid an error */
	ret = pk_backend_lock (backend_spawn->priv->backend);

	/* test search-name.sh running */
	ret = pk_backend_spawn_helper (backend_spawn, "search-name.sh", "none", "bar", NULL);
	g_assert (ret);

	/* wait for finished */
	g_main_loop_run (loop);

	/* test number of packages */
	g_assert_cmpint (number_packages, ==, 2);

	/* manually unlock as we have no engine */
	ret = pk_backend_unlock (backend_spawn->priv->backend);
	g_assert (ret);

	/* done */
	g_object_unref (backend_spawn);

	/* test we unref'd all but one of the PkBackend instances */
	refcount = G_OBJECT(backend)->ref_count;
	g_assert_cmpint (refcount, ==, 1);

	/* we ref'd it manually for checking, so we need to unref it */
	g_object_unref (backend);
	g_main_loop_unref (loop);
}
#endif

static void
pk_test_cache_func (void)
{
	PkCache *cache;

	cache = pk_cache_new ();
	g_assert (cache != NULL);

	g_object_unref (cache);
}

static void
pk_test_conf_func (void)
{
	PkConf *conf;
	gchar *text;
	gint value;

	conf = pk_conf_new ();
	g_assert (conf != NULL);

	/* get the default backend */
	text = pk_conf_get_string (conf, "DefaultBackend");
	if (text != PK_CONF_VALUE_STRING_MISSING);
	g_free (text);

	/* get a string that doesn't exist */
	text = pk_conf_get_string (conf, "FooBarBaz");
	g_assert (text == PK_CONF_VALUE_STRING_MISSING);
	g_free (text);

	/* get the shutdown timeout */
	value = pk_conf_get_int (conf, "ShutdownTimeout");
	if (value != PK_CONF_VALUE_INT_MISSING);

	/* get an int that doesn't exist */
	value = pk_conf_get_int (conf, "FooBarBaz");
	g_assert_cmpint (value, ==, PK_CONF_VALUE_INT_MISSING);

	g_object_unref (conf);
}

static void
pk_test_dbus_func (void)
{
	PkDbus *dbus;

	dbus = pk_dbus_new ();
	g_assert (dbus != NULL);

	g_object_unref (dbus);
}

#if 0
static PkNotify *notify = NULL;
static gboolean _quit = FALSE;
static gboolean _locked = FALSE;
static gboolean _restart_schedule = FALSE;

/**
 * pk_test_quit_cb:
 **/
static void
pk_test_quit_cb (PkEngine *engine, GMainLoop *loop)
{
	_quit = TRUE;
}

/**
 * pk_test_changed_cb:
 **/
static void
pk_test_changed_cb (PkEngine *engine, GMainLoop *loop)
{
	g_object_get (engine,
		      "locked", &_locked,
		      NULL);
}

/**
 * pk_test_updates_changed_cb:
 **/
static void
pk_test_updates_changed_cb (PkEngine *engine, GMainLoop *loop)
{
	g_main_loop_quit (loop)
}

/**
 * pk_test_repo_list_changed_cb:
 **/
static void
pk_test_repo_list_changed_cb (PkEngine *engine, GMainLoop *loop)
{
	g_main_loop_quit (loop)
}

/**
 * pk_test_restart_schedule_cb:
 **/
static void
pk_test_restart_schedule_cb (PkEngine *engine, GMainLoop *loop)
{
	_restart_schedule = TRUE;
	g_main_loop_quit (loop)
}

/**
 * pk_test_emit_updates_changed_cb:
 **/
static gboolean
pk_test_emit_updates_changed_cb (void)
{
	PkNotify *notify2;
	notify2 = pk_notify_new ();
	pk_notify_updates_changed (notify2);
	g_object_unref (notify2);
	return FALSE;
}

/**
 * pk_test_emit_repo_list_changed_cb:
 **/
static gboolean
pk_test_emit_repo_list_changed_cb (void)
{
	PkNotify *notify2;
	notify2 = pk_notify_new ();
	pk_notify_repo_list_changed (notify2);
	g_object_unref (notify2);
	return FALSE;
}

static void
pk_test_engine_func (void)
{
	gboolean ret;
	PkEngine *engine;
	PkBackend *backend;
	PkInhibit *inhibit;
	guint idle;
	gchar *state;
	guint elapsed;

	backend = pk_backend_new ();
	g_assert (backend != NULL);

	notify = pk_notify_new ();
	g_assert (notify != NULL);

	/* set the type, as we have no pk-main doing this for us */
	/* set the backend name */
	ret = pk_backend_set_name (backend, "dummy");
	g_assert (ret);

	/* get an engine instance */
	engine = pk_engine_new ();
	g_assert (engine != NULL);

	/* connect up signals */
	g_signal_connect (engine, "quit",
			  G_CALLBACK (pk_test_quit_cb), test);
	g_signal_connect (engine, "changed",
			  G_CALLBACK (pk_test_changed_cb), test);
	g_signal_connect (engine, "updates-changed",
			  G_CALLBACK (pk_test_updates_changed_cb), test);
	g_signal_connect (engine, "repo-list-changed",
			  G_CALLBACK (pk_test_repo_list_changed_cb), test);
	g_signal_connect (engine, "restart-schedule",
			  G_CALLBACK (pk_test_restart_schedule_cb), test);

	/* get idle at startup */
	idle = pk_engine_get_seconds_idle (engine);
	g_assert_cmpint (idle, <, 1);

	/* wait 5 seconds */
	egg_test_loop_wait (test, 5000);

	/* get idle at idle */
	idle = pk_engine_get_seconds_idle (engine);
	g_assert_cmpint (idle, <, 6);
	g_assert_cmpint (idle, >, 4);

	/* get idle after method */
	pk_engine_get_daemon_state (engine, &state, NULL);
	g_free (state);
	idle = pk_engine_get_seconds_idle (engine);
	g_assert_cmpint (idle, <, 1);

	/* force test notify updates-changed */
	g_timeout_add (25, (GSourceFunc) pk_test_emit_updates_changed_cb, test);
	xxx
	egg_test_loop_wait (test, 50);
	egg_test_loop_check ();

	/* force test notify repo-list-changed */
	g_timeout_add (25, (GSourceFunc) pk_test_emit_repo_list_changed_cb, test);
	xxx
	egg_test_loop_wait (test, 50);
	egg_test_loop_check ();

	/* force test notify wait updates-changed */
	pk_notify_wait_updates_changed (notify, 500);
	egg_test_loop_wait (test, 1000);
	g_test_timer_start ();
	elapsed = g_test_timer_elapsed ();
	g_assert_cmpfloat (elapsed, >, 0.000400);
	g_assert_cmpfloat (elapsed, <, 0.000600);

	/* test locked */
	inhibit = pk_inhibit_new ();
	pk_inhibit_add (inhibit, GUINT_TO_POINTER (999));
	if (_locked);

	/* test locked */
	pk_inhibit_remove (inhibit, GUINT_TO_POINTER (999));
	if (!_locked);
	g_object_unref (inhibit);

	/* test not locked */
	if (!_locked);

	egg_test_title_assert (test, "restart_schedule not set", !_restart_schedule);
	ret = g_file_set_contents (SBINDIR "/packagekitd", "overwrite", -1, NULL);

	egg_test_title_assert (test, "touched binary file", ret);
	egg_test_loop_wait (test, 5000);

	/* get idle after we touched the binary */
	idle = pk_engine_get_seconds_idle (engine);
	g_assert_cmpint (idle, ==, G_MAXUINT);

	egg_test_title_assert (test, "restart_schedule set", _restart_schedule);

	egg_test_title_assert (test, "not already quit", !_quit);
	/* suggest quit with no transactions (should get quit signal) */
	pk_engine_suggest_daemon_quit (engine, NULL);
	if (_quit);

	g_object_unref (backend);
	g_object_unref (notify);
	g_object_unref (engine);
}
#endif

static void
pk_test_file_monitor_func (void)
{
	PkFileMonitor *file_monitor;

	/* get a file_monitor */
	file_monitor = pk_file_monitor_new ();
	g_assert (file_monitor != NULL);
	g_object_unref (file_monitor);
}

static void
pk_test_inhibit_func (void)
{
	PkInhibit *inhibit;
	gboolean ret;

	inhibit = pk_inhibit_new ();
	if (inhibit != NULL);

	/* check we are not inhibited */
	ret = pk_inhibit_locked (inhibit);
	g_assert (!ret);

	/* add 123 */
	ret = pk_inhibit_add (inhibit, GUINT_TO_POINTER (123));
	g_assert (ret);

	/* check we are inhibited */
	ret = pk_inhibit_locked (inhibit);
	g_assert (ret);

	/* add 123 (again) */
	ret = pk_inhibit_add (inhibit, GUINT_TO_POINTER (123));
	g_assert (!ret);

	/* add 456 */
	ret = pk_inhibit_add (inhibit, GUINT_TO_POINTER (456));
	g_assert (ret);

	/* remove 123" */
	ret = pk_inhibit_remove (inhibit, GUINT_TO_POINTER (123));
	g_assert (ret);

	/* check we are still inhibited */
	ret = pk_inhibit_locked (inhibit);
	g_assert (ret);

	/* remove 456 */
	ret = pk_inhibit_remove (inhibit, GUINT_TO_POINTER (456));
	g_assert (ret);

	/* check we are not inhibited */
	ret = pk_inhibit_locked (inhibit);
	g_assert (!ret);

	g_object_unref (inhibit);
}

static void
pk_test_lsof_func (void)
{
	gboolean ret;
	PkLsof *lsof;
	GPtrArray *pids;
	gchar *files[] = { "/usr/lib/libssl3.so", NULL };

	lsof = pk_lsof_new ();
	g_assert (lsof != NULL);

	/* refresh lsof data */
	ret = pk_lsof_refresh (lsof);
	g_assert (ret);

	/* get pids for files */
	pids = pk_lsof_get_pids_for_filenames (lsof, files);
	g_assert_cmpint (pids->len, >, 0);
	g_ptr_array_unref (pids);

	g_object_unref (lsof);
}

#if 0
static void
pk_test_notify_func (void)
{
	PkNotify *notify;

	notify = pk_notify_new ();
	g_assert (notify != NULL);

	g_object_unref (notify);
}
#endif

static void
pk_test_proc_func (void)
{
	gboolean ret;
	PkProc *proc;
//	gchar *files[] = { "/sbin/udevd", NULL };

	proc = pk_proc_new ();
	g_assert (proc != NULL);

	/* refresh proc data */
	ret = pk_proc_refresh (proc);
	g_assert (ret);

	g_object_unref (proc);
}

#if 0

PkSpawnExitType mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
guint stdout_count = 0;
guint finished_count = 0;

/**
 * pk_test_exit_cb:
 **/
static void
pk_test_exit_cb (PkSpawn *spawn, PkSpawnExitType exit, GMainLoop *loop)
{
	egg_debug ("spawn exit=%i", exit);
	mexit = exit;
	finished_count++;
	g_main_loop_quit (loop)
}

/**
 * pk_test_stdout_cb:
 **/
static void
pk_test_stdout_cb (PkSpawn *spawn, const gchar *line, GMainLoop *loop)
{
	egg_debug ("stdout '%s'", line);
	stdout_count++;
}

static gboolean
cancel_cb (gpointer data)
{
	PkSpawn *spawn = PK_SPAWN(data);
	pk_spawn_kill (spawn);
	return FALSE;
}

static void
new_spawn_object (void, PkSpawn **pspawn)
{
	if (*pspawn != NULL)
		g_object_unref (*pspawn);
	*pspawn = pk_spawn_new ();
	g_signal_connect (*pspawn, "exit",
			  G_CALLBACK (pk_test_exit_cb), test);
	g_signal_connect (*pspawn, "stdout",
			  G_CALLBACK (pk_test_stdout_cb), test);
	stdout_count = 0;
}

static gboolean
idle_cb (gpointer data)
{
	void = (EggTest*) data;

	/* make sure dispatcher has closed when run idle add */
	g_assert_cmpint (mexit, ==, PK_SPAWN_EXIT_TYPE_DISPATCHER_EXIT);

	/* never repeat */
	return FALSE;
}

static void
pk_test_spawn_func (void)
{
	PkSpawn *spawn = NULL;
	gboolean ret;
	gchar *file;
	gchar *path;
	gchar **argv;
	gchar **envp;
	guint elapsed;

	/* get new object */
	new_spawn_object (test, &spawn);

	/* make sure return error for missing file */
	mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
	argv = g_strsplit ("pk-spawn-test-xxx.sh", " ", 0);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_strfreev (argv);
	g_assert (!ret);

	/* make sure finished wasn't called */
	g_assert_cmpint (mexit, ==, PK_SPAWN_EXIT_TYPE_UNKNOWN);

	/* make sure run correct helper */
	mexit = -1;
	path = egg_test_get_data_file ("pk-spawn-test.sh");
	argv = g_strsplit (path, " ", 0);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_assert (ret);
	g_free (path);
	g_strfreev (argv);

	/* wait for finished */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check ();

	/* make sure finished okay */
	g_assert_cmpint (mexit, ==, PK_SPAWN_EXIT_TYPE_SUCCESS);

	/* make sure finished was called only once */
	g_assert_cmpint (finished_count, ==, 1);

	/* make sure we got the right stdout data */
	g_assert_cmpint (stdout_count, ==, 4+11);

	/* get new object */
	new_spawn_object (test, &spawn);

	/* make sure we set the proxy */
	mexit = -1;
	path = egg_test_get_data_file ("pk-spawn-proxy.sh");
	argv = g_strsplit (path, " ", 0);
	envp = g_strsplit ("http_proxy=username:password@server:port "
			   "ftp_proxy=username:password@server:port", " ", 0);
	ret = pk_spawn_argv (spawn, argv, envp);
	g_free (path);
	g_strfreev (argv);
	g_strfreev (envp);
	g_assert (ret);

	/* wait for finished */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check ();

	/* get new object */
	new_spawn_object (test, &spawn);

	/* make sure run correct helper, and cancel it using SIGKILL */
	mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
	path = egg_test_get_data_file ("pk-spawn-test.sh");
	argv = g_strsplit (path, " ", 0);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_free (path);
	g_strfreev (argv);
	g_assert (ret);

	g_timeout_add_seconds (1, cancel_cb, spawn);
	/* wait for finished */
	egg_test_loop_wait (test, 5000);
	egg_test_loop_check ();

	/* make sure finished in SIGKILL */
	g_assert_cmpint (mexit, ==, PK_SPAWN_EXIT_TYPE_SIGKILL);

	/* get new object */
	new_spawn_object (test, &spawn);

	/* make sure dumb helper ignores SIGQUIT */
	mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
	path = egg_test_get_data_file ("pk-spawn-test.sh");
	argv = g_strsplit (path, " ", 0);
	g_object_set (spawn,
		      "allow-sigkill", FALSE,
		      NULL);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_free (path);
	g_strfreev (argv);
	g_assert (ret);

	g_timeout_add_seconds (1, cancel_cb, spawn);
	/* wait for finished */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check ();

	/* make sure finished in SIGQUIT */
	g_assert_cmpint (mexit, ==, PK_SPAWN_EXIT_TYPE_SIGQUIT);

	/* get new object */
	new_spawn_object (test, &spawn);

	/* make sure run correct helper, and SIGQUIT it */
	mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
	path = egg_test_get_data_file ("pk-spawn-test-sigquit.sh");
	argv = g_strsplit (path, " ", 0);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_free (path);
	g_strfreev (argv);
	g_assert (ret);

	g_timeout_add_seconds (1, cancel_cb, spawn);
	/* wait for finished */
	egg_test_loop_wait (test, 2000);
	egg_test_loop_check ();

	/* make sure finished in SIGQUIT */
	g_assert_cmpint (mexit, ==, PK_SPAWN_EXIT_TYPE_SIGQUIT);

	/* run lots of data for profiling */
	path = egg_test_get_data_file ("pk-spawn-test-profiling.sh");
	argv = g_strsplit (path, " ", 0);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_free (path);
	g_strfreev (argv);
	g_assert (ret);

	/* get new object */
	new_spawn_object (test, &spawn);

	/* run the dispatcher */
	mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
	file = egg_test_get_data_file ("pk-spawn-dispatcher.py");
	path = g_strdup_printf ("%s\tsearch-name\tnone\tpower manager", file);
	argv = g_strsplit (path, "\t", 0);
	envp = g_strsplit ("NETWORK=TRUE LANG=C BACKGROUND=TRUE", " ", 0);
	ret = pk_spawn_argv (spawn, argv, envp);
	g_free (file);
	g_free (path);
	g_assert (ret);

	/* wait 2+2 seconds for the dispatcher */
	/* wait 2 seconds, and make sure we are still running */
	egg_test_loop_wait (test, 4000);
	g_test_timer_start ();
	elapsed = g_test_timer_elapsed ();
	g_assert_cmpfloat (elapsed, >, 0.0003900);
	g_assert_cmpfloat (elapsed, <, 0.0004100);

	/* we got a package (+finished)? */
	g_assert_cmpint (stdout_count, ==, 2);

	/* dispatcher still alive? */
	if (spawn->priv->stdin_fd != -1);

	/* run the dispatcher with new input */
	ret = pk_spawn_argv (spawn, argv, envp);
	g_assert (ret);

	/* this may take a while */
	egg_test_loop_wait (test, 100);

	/* we got another package (not finished after bugfix)? */
	g_assert_cmpint (stdout_count, ==, 3);

	/* see if pk_spawn_exit blocks (required) */
	g_idle_add (idle_cb, test);

	/* ask dispatcher to close */
	ret = pk_spawn_exit (spawn);
	g_assert (ret);

	/* ask dispatcher to close (again, should be closing) */
	ret = pk_spawn_exit (spawn);
	g_assert (!ret);

	/* this may take a while */
	egg_test_loop_wait (test, 100);

	/* did dispatcher close? */
	g_assert_cmpint (spawn->priv->stdin_fd, ==, -1);

	/* did we get the right exit code */
	g_assert_cmpint (mexit, ==, PK_SPAWN_EXIT_TYPE_DISPATCHER_EXIT);

	/* ask dispatcher to close (again) */
	ret = pk_spawn_exit (spawn);
	g_assert (!ret);

	g_strfreev (argv);
	g_strfreev (envp);
	g_object_unref (spawn);
}
#endif

static void
pk_test_store_func (void)
{
	PkStore *store;
	gboolean ret;
	const gchar *data_string;
	guint data_uint;
	gboolean data_bool;

	store = pk_store_new ();
	if (store != NULL);

	/* set a blank string */
	ret = pk_store_set_string (store, "dave2", "");
	g_assert (ret);

	/* set a ~bool */
	ret = pk_store_set_bool (store, "roger2", FALSE);
	g_assert (ret);

	/* set a zero uint */
	ret = pk_store_set_uint (store, "linda2", 0);
	g_assert (ret);

	/* get a blank string */
	data_string = pk_store_get_string (store, "dave2");
	g_assert_cmpstr (data_string, ==, "");

	/* get a ~bool */
	data_bool = pk_store_get_bool (store, "roger2");
	if (!data_bool);

	/* get a zero uint */
	data_uint = pk_store_get_uint (store, "linda2");
	g_assert_cmpint (data_uint, ==, 0);

	/* set a string */
	ret = pk_store_set_string (store, "dave", "ania");
	g_assert (ret);

	/* set a bool */
	ret = pk_store_set_bool (store, "roger", TRUE);
	g_assert (ret);

	/* set a uint */
	ret = pk_store_set_uint (store, "linda", 999);
	g_assert (ret);

	/* get a string */
	data_string = pk_store_get_string (store, "dave");
	g_assert_cmpstr (data_string, ==, "ania");

	/* get a bool */
	data_bool = pk_store_get_bool (store, "roger");
	if (data_bool);

	/* get a uint */
	data_uint = pk_store_get_uint (store, "linda");
	g_assert_cmpint (data_uint, ==, 999);

	g_object_unref (store);
}

static void
pk_test_syslog_func (void)
{
	PkSyslog *syslog;

	syslog = pk_syslog_new ();
	g_assert (syslog != NULL);

	g_object_unref (syslog);
}

static void
pk_test_time_func (void)
{
	PkTime *pktime = NULL;
	gboolean ret;
	guint value;

	pktime = pk_time_new ();
	g_assert (pktime != NULL);

	/* get elapsed correctly at startup */
	value = pk_time_get_elapsed (pktime);
	g_assert_cmpint (value, <, 10);

	/* ignore remaining correctly */
	value = pk_time_get_remaining (pktime);
	g_assert_cmpint (value, ==, 0);

	g_usleep (1000*1000);

	/* get elapsed correctly */
	value = pk_time_get_elapsed (pktime);
	g_assert_cmpint (value, >, 900);
	g_assert_cmpint (value, <, 1100);

	/* ignore remaining correctly when not enough entries */
	value = pk_time_get_remaining (pktime);
	g_assert_cmpint (value, ==, 0);

	/* make sure we can add data */
	ret = pk_time_add_data (pktime, 10);
	g_assert (ret);

	/* make sure we can get remaining correctly */
	value = 20;
	while (value < 60) {
		pk_time_advance_clock (pktime, 2000);
		pk_time_add_data (pktime, value);
		value += 10;
	}
	value = pk_time_get_remaining (pktime);
	g_assert_cmpint (value, >, 9);
	g_assert_cmpint (value, <, 11);

	/* reset */
	g_object_unref (pktime);
	pktime = pk_time_new ();

	/* make sure we can do long times */
	value = 10;
	pk_time_add_data (pktime, 0);
	while (value < 60) {
		pk_time_advance_clock (pktime, 4*60*1000);
		pk_time_add_data (pktime, value);
		value += 10;
	}
	value = pk_time_get_remaining (pktime);
	g_assert_cmpint (value, >=, 1199);
	g_assert_cmpint (value, <=, 1201);

	g_object_unref (pktime);
}

static void
pk_test_transaction_func (void)
{
	PkTransaction *transaction = NULL;
	gboolean ret;
	GError *error = NULL;

	/* get PkTransaction object */
	transaction = pk_transaction_new ();
	g_assert (transaction != NULL);

	/* test a fail filter (null) */
	ret = pk_transaction_filter_check (NULL, &error);
	g_assert_error (error, PK_TRANSACTION_ERROR, PK_TRANSACTION_ERROR_INPUT_INVALID);
	g_assert (!ret);
	g_clear_error (&error);

	/* test a fail filter () */
	ret = pk_transaction_filter_check ("", &error);
	g_assert_error (error, PK_TRANSACTION_ERROR, PK_TRANSACTION_ERROR_INPUT_INVALID);
	g_assert (!ret);
	g_clear_error (&error);

	/* test a fail filter (;) */
	ret = pk_transaction_filter_check (";", &error);
	g_assert_error (error, PK_TRANSACTION_ERROR, PK_TRANSACTION_ERROR_INPUT_INVALID);
	g_assert (!ret);
	g_clear_error (&error);

	/* test a fail filter (invalid) */
	ret = pk_transaction_filter_check ("moo", &error);
	g_assert_error (error, PK_TRANSACTION_ERROR, PK_TRANSACTION_ERROR_INPUT_INVALID);
	g_assert (!ret);

	g_clear_error (&error);

	/* test a fail filter (invalid, multiple) */
	ret = pk_transaction_filter_check ("moo;foo", &error);
	g_assert_error (error, PK_TRANSACTION_ERROR, PK_TRANSACTION_ERROR_INPUT_INVALID);
	g_assert (!ret);
	g_clear_error (&error);

	/* test a fail filter (valid then zero length) */
	ret = pk_transaction_filter_check ("gui;;", &error);
	g_assert_error (error, PK_TRANSACTION_ERROR, PK_TRANSACTION_ERROR_INPUT_INVALID);
	g_assert (!ret);
	g_clear_error (&error);

	/* test a pass filter (none) */
	ret = pk_transaction_filter_check ("none", &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_clear_error (&error);

	/* test a pass filter (single) */
	ret = pk_transaction_filter_check ("gui", &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_clear_error (&error);

	/* test a pass filter (multiple) */
	ret = pk_transaction_filter_check ("devel;~gui", &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_clear_error (&error);

	/* test a pass filter (multiple2) */
	ret = pk_transaction_filter_check ("~gui;~installed", &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_clear_error (&error);

	/* validate incorrect text */
	ret = pk_transaction_strvalidate ("richard$hughes", &error);
	g_assert_error (error, PK_TRANSACTION_ERROR, PK_TRANSACTION_ERROR_INPUT_INVALID);
	g_assert (!ret);
	g_clear_error (&error);

	/* validate correct text */
	ret = pk_transaction_strvalidate ("richardhughes", &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_clear_error (&error);

	g_object_unref (transaction);
}

static void
pk_test_transaction_db_func (void)
{
	PkTransactionDb *db;
	guint value;
	gchar *tid;
	gboolean ret;
	gdouble ms;
	gchar *proxy_http = NULL;
	gchar *proxy_ftp = NULL;
	gchar *root = NULL;
	guint seconds;

	/* remove the self check file */
#if PK_BUILD_LOCAL
	ret = g_file_test (PK_TRANSACTION_DB_FILE, G_FILE_TEST_EXISTS);
	if (ret) {
		/* remove old local database */
		egg_warning ("Removing %s", PK_TRANSACTION_DB_FILE);
		value = g_unlink (PK_TRANSACTION_DB_FILE);
		g_assert (value == 0);
	}
#endif

	/* check we created quickly */
	g_test_timer_start ();
	db = pk_transaction_db_new ();
	ms = g_test_timer_elapsed ();
	g_assert_cmpfloat (ms, <, 1.5);
	g_object_unref (db);

	/* check we opened quickly */
	g_test_timer_start ();
	db = pk_transaction_db_new ();
	ms = g_test_timer_elapsed ();
	g_assert_cmpfloat (ms, <, 0.1);

	/* do we get the correct time on a blank database */
	value = pk_transaction_db_action_time_since (db, PK_ROLE_ENUM_REFRESH_CACHE);
	g_assert_cmpint (value, ==, G_MAXUINT);

	/* get an tid object */
	g_test_timer_start ();
	tid = pk_transaction_db_generate_id (db);
	ms = g_test_timer_elapsed ();
	g_assert_cmpfloat (ms, <, 0.001);
	g_free (tid);

	/* get an tid object (no wait) */
	g_test_timer_start ();
	tid = pk_transaction_db_generate_id (db);
	ms = g_test_timer_elapsed ();
	g_assert_cmpfloat (ms, <, 0.005);
	g_free (tid);

	/* set the correct time */
	ret = pk_transaction_db_action_time_reset (db, PK_ROLE_ENUM_REFRESH_CACHE);
	g_assert (ret);

	/* do the deferred write */
	g_test_timer_start ();
	while (g_main_context_pending (NULL))
		g_main_context_iteration (NULL, TRUE);
	ms = g_test_timer_elapsed ();
	g_assert_cmpfloat (ms, >, 0.001);

	g_usleep (2*1000*1000);

	/* do we get the correct time */
	value = pk_transaction_db_action_time_since (db, PK_ROLE_ENUM_REFRESH_CACHE);
	g_assert_cmpint (value, >, 1);
	g_assert_cmpint (value, <=, 4);

	/* can we set the proxies */
	ret = pk_transaction_db_set_proxy (db, 500, "session1", "127.0.0.1:80", "127.0.0.1:21");
	g_assert (ret);

	/* can we set the proxies (overwrite) */
	ret = pk_transaction_db_set_proxy (db, 500, "session1", "127.0.0.1:8000", "127.0.0.1:21");
	g_assert (ret);

	/* can we get the proxies (non-existant user) */
	ret = pk_transaction_db_get_proxy (db, 501, "session1", &proxy_http, &proxy_ftp);
	g_assert (!ret);
	g_assert_cmpstr (proxy_http, ==, NULL);
	g_assert_cmpstr (proxy_ftp, ==, NULL);

	/* can we get the proxies (non-existant session) */
	ret = pk_transaction_db_get_proxy (db, 500, "session2", &proxy_http, &proxy_ftp);
	g_assert (!ret);
	g_assert_cmpstr (proxy_http, ==, NULL);
	g_assert_cmpstr (proxy_ftp, ==, NULL);

	/* can we get the proxies (match) */
	ret = pk_transaction_db_get_proxy (db, 500, "session1", &proxy_http, &proxy_ftp);
	g_assert (ret);
	g_assert_cmpstr (proxy_http, ==, "127.0.0.1:8000");
	g_assert_cmpstr (proxy_ftp, ==, "127.0.0.1:21");

	/* can we set the root */
	ret = pk_transaction_db_set_root (db, 500, "session1", "/mnt/chroot");
	g_assert (ret);

	/* can we set the root (overwrite) */
	ret = pk_transaction_db_set_root (db, 500, "session1", "/mnt/chroot2");
	g_assert (ret);

	/* can we get the root (non-existant user) */
	ret = pk_transaction_db_get_root (db, 501, "session1", &root);
	g_assert (!ret);
	g_assert_cmpstr (root, ==, NULL);

	/* can we get the root (match) */
	ret = pk_transaction_db_get_root (db, 500, "session1", &root);
	g_assert_cmpstr (root, ==, "/mnt/chroot2");

	g_free (root);
	g_free (proxy_http);
	g_free (proxy_ftp);
	g_object_unref (db);
}

static void
pk_test_transaction_extra_func (void)
{
	PkTransactionExtra *extra;

	extra = pk_transaction_extra_new ();
	g_assert (extra != NULL);

	g_object_unref (extra);
}

#if 0
static PkTransactionDb *db = NULL;

/**
 * pk_test_transaction_list_test_finished_cb:
 **/
static void
pk_test_transaction_list_test_finished_cb (PkTransaction *transaction, const gchar *exit_text, guint time, GMainLoop *loop)
{
	g_main_loop_quit (loop)
}

/**
 * pk_test_transaction_list_test_delay_cb:
 **/
static void
pk_test_transaction_list_test_delay_cb (GMainLoop *loop)
{
	egg_debug ("quitting loop");
	g_main_loop_quit (loop)
}

/**
 * pk_test_transaction_list_test_get_item:
 **/
static PkTransactionItem *
pk_test_transaction_list_test_get_item (PkTransactionList *tlist)
{
	PkTransactionItem *item;
	gchar *tid;

	/* get tid */
	tid = pk_transaction_db_generate_id (db);

	/* create PkTransaction instance */
	pk_transaction_list_create (tlist, tid, ":0", NULL);
	item = pk_transaction_list_get_from_tid (tlist, tid);
	g_free (tid);

	/* return object */
	return item;
}

static void
pk_test_transaction_list_func (void)
{
	PkTransactionList *tlist;
	PkCache *cache;
	gboolean ret;
	gchar *tid;
	guint size;
	gchar **array;
	PkTransactionItem *item;
	PkTransactionItem *item1;
	PkTransactionItem *item2;
	PkTransactionItem *item3;

	/* remove the self check file */
#if PK_BUILD_LOCAL
	ret = g_file_test ("./transactions.db", G_FILE_TEST_EXISTS);
	if (ret) {
		/* remove old local database */
		egg_warning ("Removing %s", "./transactions.db");
		size = g_unlink ("./transactions.db");
		g_assert (size == 0);
	}
#endif

	/* we get a cache object to reproduce the engine having it ref'd */
	cache = pk_cache_new ();
	db = pk_transaction_db_new ();

	/* get a transaction list object */
	tlist = pk_transaction_list_new ();
	g_assert (tlist != NULL);

	/* make sure we get a valid tid */
	tid = pk_transaction_db_generate_id (db);
	if (tid != NULL);

	/* create a transaction object */
	ret = pk_transaction_list_create (tlist, tid, ":0", NULL);
	g_assert (ret);

	/* make sure we get the right object back */
	item = pk_transaction_list_get_from_tid (tlist, tid);
	if (item != NULL &&
	    (g_strcmp0 (item->tid, ==, tid) &&
	    item->transaction != NULL);

	/* make sure item has correct flags */
	g_assert (!item->running);
	g_assert (!item->committed);
	g_assert (!item->finished);

	/* get size one we have in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 1);

	/* get transactions (committed, not finished) in progress */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 0);
	g_strfreev (array);

	/* add again the same tid (should fail) */
	ret = pk_transaction_list_create (tlist, tid, ":0", NULL);
	g_assert (!ret);

	/* remove without ever committing */
	ret = pk_transaction_list_remove (tlist, tid);
	g_assert (ret);

	/* get size none we have in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 0);

	/* get a new tid */
	g_free (tid);
	tid = pk_transaction_db_generate_id (db);

	/* create another item */
	ret = pk_transaction_list_create (tlist, tid, ":0", NULL);
	g_assert (ret);

	PkBackend *backend;
	backend = pk_backend_new ();
	/* try to load a valid backend */
	ret = pk_backend_set_name (backend, "dummy");
	g_assert (ret);

	/* lock an valid backend */
	ret = pk_backend_lock (backend);
	g_assert (ret);

	/* get from db */
	item = pk_transaction_list_get_from_tid (tlist, tid);
	if (item != NULL &&
	    (g_strcmp0 (item->tid, ==, tid) &&
	    item->transaction != NULL);

	g_signal_connect (item->transaction, "finished",
			  G_CALLBACK (pk_transaction_list_test_finished_cb), test);

	/* this tests the run-on-commit action */
	pk_transaction_get_updates (item->transaction, "none", NULL);

	/* make sure item has correct flags */
	g_assert (item->running);
	g_assert (item->committed);
	g_assert (!item->finished);

	/* get present role */
	ret = pk_transaction_list_role_present (tlist, PK_ROLE_ENUM_GET_UPDATES);
	g_assert (ret);

	/* get non-present role */
	ret = pk_transaction_list_role_present (tlist, PK_ROLE_ENUM_SEARCH_NAME);
	g_assert (!ret);

	/* get size we have in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 1);

	/* get transactions (committed, not finished) in progress */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 1);
	g_strfreev (array);

	/* wait for Finished */
	egg_test_loop_wait (test, 2000);
	egg_test_loop_check ();

	/* get size one we have in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 1);

	/* get transactions (committed, not finished) in progress (none) */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 0);
	g_strfreev (array);

	/* remove already removed */
	ret = pk_transaction_list_remove (tlist, tid);
	g_assert (!ret);

	/* wait for Cleanup */
	g_timeout_add_seconds (5, (GSourceFunc) pk_transaction_list_test_delay_cb, test);
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check ();

	/* make sure queue empty */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 0);

	g_free (tid);

	item = pk_transaction_list_test_get_item (tlist);
	g_signal_connect (item->transaction, "finished",
			  G_CALLBACK (pk_transaction_list_test_finished_cb), test);

	pk_transaction_get_updates (item->transaction, "none", NULL);

	/* wait for cached results*/
	egg_test_loop_wait (test, 1000);
	egg_test_loop_check ();

	/* make sure item has correct flags */
	g_assert (!item->running);
	g_assert (item->committed);
	g_assert (item->finished);

	/* get transactions (committed, not finished) in progress (none, as cached) */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 0);
	g_strfreev (array);

	/* get size we have in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 1);

	/* wait for Cleanup */
	g_timeout_add_seconds (5, (GSourceFunc) pk_transaction_list_test_delay_cb, test);
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check ();

	/* get transactions (committed, not finished) in progress (none, as cached) */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 0);
	g_strfreev (array);

	/* get size we have in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 0);

	/* create three instances in list */
	item1 = pk_transaction_list_test_get_item (tlist);
	item2 = pk_transaction_list_test_get_item (tlist);
	item3 = pk_transaction_list_test_get_item (tlist);

	/* get all items in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 3);

	/* get transactions (committed, not finished) committed */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 0);
	g_strfreev (array);

	g_signal_connect (item1->transaction, "finished",
			  G_CALLBACK (pk_transaction_list_test_finished_cb), test);
	g_signal_connect (item2->transaction, "finished",
			  G_CALLBACK (pk_transaction_list_test_finished_cb), test);
	g_signal_connect (item3->transaction, "finished",
			  G_CALLBACK (pk_transaction_list_test_finished_cb), test);

	/* this starts one action */
	array = g_strsplit ("dave", " ", -1);
	pk_transaction_search_details (item1->transaction, "none", array, NULL);
	g_strfreev (array);

	/* this should be chained after the first action completes */
	array = g_strsplit ("power", " ", -1);
	pk_transaction_search_names (item2->transaction, "none", array, NULL);
	g_strfreev (array);

	/* this starts be chained after the second action completes */
	array = g_strsplit ("paul", " ", -1);
	pk_transaction_search_details (item3->transaction, "none", array, NULL);
	g_strfreev (array);

	/* get transactions (committed, not finished) in progress (all) */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 3);
	g_strfreev (array);

	/* wait for first action */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check ();

	/* get all items in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 3);

	/* get transactions (committed, not finished) (two, first one finished) */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 2);
	g_strfreev (array);

	/* make sure item1 has correct flags */
	if (item1->running == FALSE && item1->committed == TRUE && item1->finished == TRUE);

	/* make sure item2 has correct flags */
	if (item2->running == TRUE && item2->committed == TRUE && item2->finished == FALSE);

	/* make sure item3 has correct flags */
	if (item3->running == FALSE && item3->committed == TRUE && item3->finished == FALSE);

	/* wait for second action */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check ();

	/* get all items in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 3);

	/* get transactions (committed, not finished) in progress (one) */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 1);
	g_strfreev (array);

	/* make sure item1 has correct flags */
	if (item1->running == FALSE && item1->committed == TRUE && item1->finished == TRUE);

	/* make sure item2 has correct flags */
	if (item2->running == FALSE && item2->committed == TRUE && item2->finished == TRUE);

	/* make sure item3 has correct flags */
	if (item3->running == TRUE && item3->committed == TRUE && item3->finished == FALSE);

	/* wait for third action */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check ();

	/* get all items in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 3);

	/* get transactions (committed, not finished) in progress (none) */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 0);
	g_strfreev (array);

	/* make sure item1 has correct flags */
	if (item1->running == FALSE && item1->committed == TRUE && item1->finished == TRUE);

	/* make sure item2 has correct flags */
	if (item2->running == FALSE && item2->committed == TRUE && item2->finished == TRUE);

	/* make sure item3 has correct flags */
	if (item3->running == FALSE && item3->committed == TRUE && item3->finished == TRUE);

	/* wait for Cleanup */
	g_timeout_add_seconds (5, (GSourceFunc) pk_transaction_list_test_delay_cb, test);
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check ();

	/* get both items in queue */
	size = pk_transaction_list_get_size (tlist);
	g_assert_cmpint (size, ==, 0);

	/* get transactions (committed, not finished) in progress (neither - again) */
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	g_assert_cmpint (size, ==, 0);
	g_strfreev (array);

	g_object_unref (tlist);
	g_object_unref (backend);
	g_object_unref (cache);
	g_object_unref (db);
}
#endif

int
main (int argc, char **argv)
{
	if (! g_thread_supported ())
		g_thread_init (NULL);
	g_type_init ();
	egg_debug_init (&argc, &argv);
	g_test_init (&argc, &argv, NULL);

	/* egg */
//	egg_string_func ();

	/* components */
	g_test_add_func ("/packagekit/proc", pk_test_proc_func);
	g_test_add_func ("/packagekit/lsof", pk_test_lsof_func);
	g_test_add_func ("/packagekit/file-monitor", pk_test_file_monitor_func);
	g_test_add_func ("/packagekit/time", pk_test_time_func);
	g_test_add_func ("/packagekit/dbus", pk_test_dbus_func);
	g_test_add_func ("/packagekit/syslog", pk_test_dbus_func);
	g_test_add_func ("/packagekit/conf", pk_test_conf_func);
	g_test_add_func ("/packagekit/cache", pk_test_conf_func);
	g_test_add_func ("/packagekit/store", pk_test_store_func);
	g_test_add_func ("/packagekit/inhibit", pk_test_inhibit_func);
//	g_test_add_func ("/packagekit/spawn", pk_test_spawn_func);
	g_test_add_func ("/packagekit/transaction", pk_test_transaction_func);
//	g_test_add_func ("/packagekit/transaction-list", pk_test_transaction_list_func);
	g_test_add_func ("/packagekit/transaction-db", pk_test_transaction_db_func);
	g_test_add_func ("/packagekit/transaction-extra", pk_test_transaction_extra_func);

	/* backend stuff */
//	g_test_add_func ("/packagekit/backend", pk_test_backend_func);
//	g_test_add_func ("/packagekit/backend_spawn", pk_test_backend_spawn_func);

	/* system */
//	g_test_add_func ("/packagekit/engine", pk_test_engine_func);

	return g_test_run ();
}


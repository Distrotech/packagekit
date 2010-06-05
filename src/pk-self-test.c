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

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include "pk-backend-internal.h"
#include "pk-cache.h"
#include "pk-transaction-db.h"

static guint number_messages = 0;
static guint number_packages = 0;

/**
 * pk_test_backend_test_message_cb:
 **/
static void
pk_test_backend_test_message_cb (PkBackend *backend, PkMessageEnum message, const gchar *details, gpointer data)
{
	egg_debug ("details=%s", details);
	number_messages++;
}

/**
 * pk_test_backend_test_finished_cb:
 **/
static void
pk_test_backend_test_finished_cb (PkBackend *backend, PkExitEnum exit, GMainLoop *loop)
{
	g_main_loop_quit (loop)
}

/**
 * pk_test_backend_test_watch_file_cb:
 **/
static void
pk_test_backend_test_watch_file_cb (PkBackend *backend, GMainLoop *loop)
{
	g_main_loop_quit (loop)
}

static gboolean
pk_test_backend_test_func_true (PkBackend *backend)
{
	g_usleep (1000*1000);
	/* trigger duplicate test */
	pk_backend_package (backend, PK_INFO_ENUM_AVAILABLE, "vips-doc;7.12.4-2.fc8;noarch;linva", "The vips documentation package.");
	pk_backend_package (backend, PK_INFO_ENUM_AVAILABLE, "vips-doc;7.12.4-2.fc8;noarch;linva", "The vips documentation package.");
	pk_backend_finished (backend);
	return TRUE;
}

static gboolean
pk_test_backend_test_func_immediate_false (PkBackend *backend)
{
	pk_backend_finished (backend);
	return FALSE;
}

/**
 * pk_test_backend_test_package_cb:
 **/
static void
pk_test_backend_test_package_cb (PkBackend *backend, PkPackage *item, GMainLoop *loop)
{
	egg_debug ("package:%s", pk_package_get_id (item));
	number_packages++;
}

static void
pk_test_backend_func (EggTest *test)
{
	PkBackend *backend;
	PkConf *conf;
	gchar *text;
	gchar *text_safe;
	gboolean ret;
	const gchar *filename;
	gboolean developer_mode;

	/************************************************************
	 ****************       REPLACE CHARS      ******************
	 ************************************************************/
	egg_test_title (test, "test replace unsafe (okay)");
	text_safe = pk_backend_strsafe ("Richard Hughes");
	if (g_strcmp0 (text_safe, "Richard Hughes") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed the replace unsafe '%s'", text_safe);
	g_free (text_safe);

	egg_test_title (test, "test replace UTF8 unsafe (okay)");
	text_safe = pk_backend_strsafe ("Gölas");
	if (g_strcmp0 (text_safe, "Gölas") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed the replace unsafe '%s'", text_safe);
	g_free (text_safe);

	egg_test_title (test, "test replace unsafe (one invalid)");
	text_safe = pk_backend_strsafe ("Richard\rHughes");
	if (g_strcmp0 (text_safe, "Richard Hughes") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed the replace unsafe '%s'", text_safe);
	g_free (text_safe);

	egg_test_title (test, "test replace unsafe (multiple invalid)");
	text_safe = pk_backend_strsafe (" Richard\rHughes\f");
	if (g_strcmp0 (text_safe, " Richard Hughes ") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed the replace unsafe '%s'", text_safe);
	g_free (text_safe);

	egg_test_title (test, "get an backend");
	backend = pk_backend_new ();
	if (backend != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, NULL);

	/* connect */
	g_signal_connect (backend, "package",
			  G_CALLBACK (pk_test_backend_test_package_cb), test);

	egg_test_title (test, "create a config file");
	filename = "/tmp/dave";
	ret = g_file_set_contents (filename, "foo", -1, NULL);
	if (ret) {
		egg_test_success (test, "set contents");
	} else
		egg_test_failed (test, NULL);

	egg_test_title (test, "set up a watch file on a config file");
	ret = pk_backend_watch_file (backend, filename, pk_backend_test_watch_file_cb, test);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "eula valid");

	egg_test_title (test, "change the config file");
	ret = g_file_set_contents (filename, "bar", -1, NULL);
	if (ret) {
		egg_test_success (test, "set contents");
	} else
		egg_test_failed (test, NULL);

	/* wait for config file change */
	egg_test_loop_wait (test, 2000);
	egg_test_loop_check (test);

	egg_test_title (test, "delete the config file");
	ret = g_unlink (filename);
	egg_test_assert (test, !ret);

	g_signal_connect (backend, "message", G_CALLBACK (pk_test_backend_test_message_cb), NULL);
	g_signal_connect (backend, "finished", G_CALLBACK (pk_test_backend_test_finished_cb), test);

	egg_test_title (test, "get eula that does not exist");
	ret = pk_backend_is_eula_valid (backend, "license_foo");
	if (!ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "eula valid");

	egg_test_title (test, "accept eula");
	ret = pk_backend_accept_eula (backend, "license_foo");
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "eula was not accepted");

	egg_test_title (test, "get eula that does exist");
	ret = pk_backend_is_eula_valid (backend, "license_foo");
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "eula valid");

	egg_test_title (test, "accept eula (again)");
	ret = pk_backend_accept_eula (backend, "license_foo");
	if (!ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "eula was accepted twice");

	egg_test_title (test, "load an invalid backend");
	ret = pk_backend_set_name (backend, "invalid");
	if (ret == FALSE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, NULL);

	egg_test_title (test, "try to load a valid backend");
	ret = pk_backend_set_name (backend, "dummy");
	egg_test_assert (test, ret);

	egg_test_title (test, "load an valid backend again");
	ret = pk_backend_set_name (backend, "dummy");
	if (ret == FALSE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "loaded twice");

	egg_test_title (test, "lock an valid backend");
	ret = pk_backend_lock (backend);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to lock");

	egg_test_title (test, "lock a backend again");
	ret = pk_backend_lock (backend);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "locked twice should succeed");

	egg_test_title (test, "check we are out of init");
	if (backend->priv->during_initialize == FALSE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "not out of init");

	egg_test_title (test, "get backend name");
	text = pk_backend_get_name (backend);
	if (g_strcmp0 (text, "dummy") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid name %s", text);
	g_free (text);

	egg_test_title (test, "unlock an valid backend");
	ret = pk_backend_unlock (backend);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to unlock");

	egg_test_title (test, "unlock an valid backend again");
	ret = pk_backend_unlock (backend);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "unlocked twice, should succeed");

	egg_test_title (test, "check we are not finished");
	if (backend->priv->finished == FALSE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "we did not clear finish!");

	egg_test_title (test, "check we have no error");
	if (backend->priv->set_error == FALSE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "an error has already been set");

	egg_test_title (test, "lock again");
	ret = pk_backend_lock (backend);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to unlock");

	egg_test_title (test, "wait for a thread to return true");
	ret = pk_backend_thread_create (backend, pk_test_backend_test_func_true);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wait for a thread failed");

	/* wait for Finished */
	egg_test_loop_wait (test, 2000);
	egg_test_loop_check (test);

	egg_test_title (test, "check duplicate filter");
	if (number_packages == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong number of packages: %i", number_packages);

	/* reset */
	pk_backend_reset (backend);

	egg_test_title (test, "wait for a thread to return false (straight away)");
	ret = pk_backend_thread_create (backend, pk_test_backend_test_func_immediate_false);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "returned false!");

	/* wait for Finished */
	egg_test_loop_wait (test, PK_BACKEND_FINISHED_TIMEOUT_GRACE + 100);
	egg_test_loop_check (test);

	pk_backend_reset (backend);
	pk_backend_error_code (backend, PK_ERROR_ENUM_GPG_FAILURE, "test error");

	/* wait for finished */
	egg_test_loop_wait (test, PK_BACKEND_FINISHED_ERROR_TIMEOUT + 400);
	egg_test_loop_check (test);

	egg_test_title (test, "get allow cancel after reset");
	pk_backend_reset (backend);
	ret = pk_backend_get_allow_cancel (backend);
	egg_test_assert (test, !ret);

	egg_test_title (test, "set allow cancel TRUE");
	ret = pk_backend_set_allow_cancel (backend, TRUE);
	egg_test_assert (test, ret);

	egg_test_title (test, "set allow cancel TRUE (repeat)");
	ret = pk_backend_set_allow_cancel (backend, TRUE);
	egg_test_assert (test, !ret);

	egg_test_title (test, "set allow cancel FALSE");
	ret = pk_backend_set_allow_cancel (backend, FALSE);
	egg_test_assert (test, ret);

	egg_test_title (test, "set allow cancel FALSE (after reset)");
	pk_backend_reset (backend);
	ret = pk_backend_set_allow_cancel (backend, FALSE);
	egg_test_assert (test, ret);

	/* if running in developer mode, then expect a Message */
	conf = pk_conf_new ();
	developer_mode = pk_conf_get_bool (conf, "DeveloperMode");
	g_object_unref (conf);
	if (developer_mode) {
			egg_test_title (test, "check we enforce finished after error_code");
		if (number_messages == 1)
			egg_test_success (test, NULL);
		else
			egg_test_failed (test, "we messaged %i times!", number_messages);
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
pk_test_backend_test_spawn_func (EggTest *test)
{
	PkBackendSpawn *backend_spawn;
	PkBackend *backend;
	const gchar *text;
	guint refcount;
	gboolean ret;
	gchar *uri;
	gchar **array;

	loop = g_main_loop_new (NULL, FALSE);

	egg_test_title (test, "va_list_to_argv single");
	array = pk_test_backend_spawn_va_list_to_argv_test ("richard", NULL);
	if (g_strcmp0 (array[0], "richard") == 0 &&
	    array[1] == NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect array '%s'", array[0]);
	g_strfreev (array);

	egg_test_title (test, "va_list_to_argv triple");
	array = pk_test_backend_spawn_va_list_to_argv_test ("richard", "phillip", "hughes", NULL);
	if (g_strcmp0 (array[0], "richard") == 0 &&
	    g_strcmp0 (array[1], "phillip") == 0 &&
	    g_strcmp0 (array[2], "hughes") == 0 &&
	    array[3] == NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect array '%s','%s','%s'", array[0], array[1], array[2]);
	g_strfreev (array);

	egg_test_title (test, "get an backend_spawn");
	backend_spawn = pk_backend_spawn_new ();
	egg_test_assert (test, backend_spawn != NULL);

	/* private copy for unref testing */
	backend = backend_spawn->priv->backend;
	/* incr ref count so we don't kill the object */
	g_object_ref (backend);

	egg_test_title (test, "get backend name");
	text = pk_backend_spawn_get_name (backend_spawn);
	if (text == NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid name %s", text);

	egg_test_title (test, "set backend name");
	ret = pk_backend_spawn_set_name (backend_spawn, "test_spawn");
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid set name");

	egg_test_title (test, "get backend name");
	text = pk_backend_spawn_get_name (backend_spawn);
	if (g_strcmp0 (text, "test_spawn") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid name %s", text);

	/* needed to avoid an error */
	ret = pk_backend_set_name (backend_spawn->priv->backend, "test_spawn");
	ret = pk_backend_lock (backend_spawn->priv->backend);

	/************************************************************
	 **********       Check parsing common error      ***********
	 ************************************************************/
	egg_test_title (test, "test pk_backend_spawn_parse_stdout Percentage1");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "percentage\t0");
	egg_test_assert (test, ret);

	egg_test_title (test, "test pk_backend_spawn_parse_stdout Percentage2");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "percentage\tbrian");
	egg_test_assert (test, !ret);

	egg_test_title (test, "test pk_backend_spawn_parse_stdout Percentage3");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "percentage\t12345");
	egg_test_assert (test, !ret);

	egg_test_title (test, "test pk_backend_spawn_parse_stdout Percentage4");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "percentage\t");
	egg_test_assert (test, !ret);

	egg_test_title (test, "test pk_backend_spawn_parse_stdout Percentage5");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "percentage");
	egg_test_assert (test, !ret);

	egg_test_title (test, "test pk_backend_spawn_parse_stdout Subpercentage");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "subpercentage\t17");
	egg_test_assert (test, ret);

	egg_test_title (test, "test pk_backend_spawn_parse_stdout NoPercentageUpdates");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "no-percentage-updates");
	egg_test_assert (test, ret);

	egg_test_title (test, "test pk_backend_spawn_parse_stdout failure");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "error\tnot-present-woohoo\tdescription text");
	if (!ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not detect incorrect enum");

	egg_test_title (test, "test pk_backend_spawn_parse_stdout Status");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "status\tquery");
	egg_test_assert (test, ret);

	egg_test_title (test, "test pk_backend_spawn_parse_stdout RequireRestart");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "requirerestart\tsystem\tgnome-power-manager;0.0.1;i386;data");
	egg_test_assert (test, ret);

	egg_test_title (test, "test pk_backend_spawn_parse_stdout RequireRestart invalid enum");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "requirerestart\tmooville\tgnome-power-manager;0.0.1;i386;data");
	if (!ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not detect incorrect enum");

	egg_test_title (test, "test pk_backend_spawn_parse_stdout RequireRestart invalid PackageId");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "requirerestart\tsystem\tdetails about the restart");
	if (!ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not detect incorrect package id");

	egg_test_title (test, "test pk_backend_spawn_parse_stdout AllowUpdate1");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "allow-cancel\ttrue");
	egg_test_assert (test, ret);

	egg_test_title (test, "test pk_backend_spawn_parse_stdout AllowUpdate2");
	ret = pk_backend_spawn_parse_stdout (backend_spawn, "allow-cancel\tbrian");
	egg_test_assert (test, !ret);

	/************************************************************
	 **********         Check uri conversion          ***********
	 ************************************************************/
	egg_test_title (test, "convert proxy uri (bare)");
	uri = pk_backend_spawn_convert_uri ("username:password@server:port");
	egg_test_assert (test, (g_strcmp0 (uri, "http://username:password@server:port/") == 0));
	g_free (uri);

	egg_test_title (test, "convert proxy uri (full)");
	uri = pk_backend_spawn_convert_uri ("http://username:password@server:port/");
	egg_test_assert (test, (g_strcmp0 (uri, "http://username:password@server:port/") == 0));
	g_free (uri);

	egg_test_title (test, "convert proxy uri (partial)");
	uri = pk_backend_spawn_convert_uri ("ftp://username:password@server:port");
	egg_test_assert (test, (g_strcmp0 (uri, "ftp://username:password@server:port/") == 0));
	g_free (uri);

	/************************************************************
	 **********        Check parsing common out       ***********
	 ************************************************************/
	egg_test_title (test, "test pk_backend_spawn_parse_common_out Package");
	ret = pk_backend_spawn_parse_stdout (backend_spawn,
		"package\tinstalled\tgnome-power-manager;0.0.1;i386;data\tMore useless software");
	egg_test_assert (test, ret);

	egg_test_title (test, "manually unlock as we have no engine");
	ret = pk_backend_unlock (backend_spawn->priv->backend);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not unlock");

	/* reset */
	g_object_unref (backend_spawn);

	egg_test_title (test, "test we unref'd all but one of the PkBackend instances");
	refcount = G_OBJECT(backend)->ref_count;
	if (refcount == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "refcount invalid %i", refcount);

	/* new */
	backend_spawn = pk_backend_spawn_new ();

	egg_test_title (test, "set backend name");
	ret = pk_backend_spawn_set_name (backend_spawn, "test_spawn");
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid set name");

	/* so we can spin until we finish */
	g_signal_connect (backend_spawn->priv->backend, "finished",
			  G_CALLBACK (pk_backend_spawn_test_finished_cb), backend_spawn);
	/* so we can count the returned packages */
	g_signal_connect (backend_spawn->priv->backend, "package",
			  G_CALLBACK (pk_backend_spawn_test_package_cb), backend_spawn);

	/* needed to avoid an error */
	ret = pk_backend_lock (backend_spawn->priv->backend);

	/************************************************************
	 **********          Use a spawned helper         ***********
	 ************************************************************/
	egg_test_title (test, "test search-name.sh running");
	ret = pk_backend_spawn_helper (backend_spawn, "search-name.sh", "none", "bar", NULL);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "cannot spawn search-name.sh");

	/* wait for finished */
	g_main_loop_run (loop);

	egg_test_title (test, "test number of packages");
	if (number_packages == 2)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong number of packages %i", number_packages);

	egg_test_title (test, "manually unlock as we have no engine");
	ret = pk_backend_unlock (backend_spawn->priv->backend);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not unlock");

	/* done */
	g_object_unref (backend_spawn);

	egg_test_title (test, "test we unref'd all but one of the PkBackend instances");
	refcount = G_OBJECT(backend)->ref_count;
	if (refcount == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "refcount invalid %i", refcount);

	/* we ref'd it manually for checking, so we need to unref it */
	g_object_unref (backend);
	g_main_loop_unref (loop);
}


static void
pk_test_cache_func (EggTest *test)
{
	PkCache *cache;

	egg_test_title (test, "get an instance");
	cache = pk_cache_new ();
	if (cache != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, NULL);

	g_object_unref (cache);
}


static void
pk_test_conf_func (EggTest *test)
{
	PkConf *conf;
	gchar *text;
	gint value;

	egg_test_title (test, "get an instance");
	conf = pk_conf_new ();
	egg_test_assert (test, conf != NULL);

	egg_test_title (test, "get the default backend");
	text = pk_conf_get_string (conf, "DefaultBackend");
	if (text != PK_CONF_VALUE_STRING_MISSING)
		egg_test_success (test, "got default backend '%s'", text);
	else
		egg_test_failed (test, "got NULL!");
	g_free (text);

	egg_test_title (test, "get a string that doesn't exist");
	text = pk_conf_get_string (conf, "FooBarBaz");
	if (text == PK_CONF_VALUE_STRING_MISSING)
		egg_test_success (test, "got NULL", text);
	else
		egg_test_failed (test, "got return value '%s'", text);
	g_free (text);

	egg_test_title (test, "get the shutdown timeout");
	value = pk_conf_get_int (conf, "ShutdownTimeout");
	if (value != PK_CONF_VALUE_INT_MISSING)
		egg_test_success (test, "got ShutdownTimeout '%i'", value);
	else
		egg_test_failed (test, "got %i", value);

	egg_test_title (test, "get an int that doesn't exist");
	value = pk_conf_get_int (conf, "FooBarBaz");
	if (value == PK_CONF_VALUE_INT_MISSING)
		egg_test_success (test, "got %i", value);
	else
		egg_test_failed (test, "got return value '%i'", value);

	g_object_unref (conf);
}


static void
pk_test_dbus_func (EggTest *test)
{
	PkDbus *dbus;

	egg_test_title (test, "get an instance");
	dbus = pk_dbus_new ();
	egg_test_assert (test, dbus != NULL);

	g_object_unref (dbus);
}


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
pk_test_emit_updates_changed_cb (EggTest *test)
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
pk_test_emit_repo_list_changed_cb (EggTest *test)
{
	PkNotify *notify2;
	notify2 = pk_notify_new ();
	pk_notify_repo_list_changed (notify2);
	g_object_unref (notify2);
	return FALSE;
}

static void
pk_test_engine_func (EggTest *test)
{
	gboolean ret;
	PkEngine *engine;
	PkBackend *backend;
	PkInhibit *inhibit;
	guint idle;
	gchar *state;
	guint elapsed;

	egg_test_title (test, "get a backend instance");
	backend = pk_backend_new ();
	egg_test_assert (test, backend != NULL);

	egg_test_title (test, "get a notify instance");
	notify = pk_notify_new ();
	egg_test_assert (test, notify != NULL);

	/* set the type, as we have no pk-main doing this for us */
	egg_test_title (test, "set the backend name");
	ret = pk_backend_set_name (backend, "dummy");
	egg_test_assert (test, ret);

	egg_test_title (test, "get an engine instance");
	engine = pk_engine_new ();
	egg_test_assert (test, engine != NULL);

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

	/************************************************************
	 **********           GET IDLE TIMES              ***********
	 ************************************************************/
	egg_test_title (test, "get idle at startup");
	idle = pk_engine_get_seconds_idle (engine);
	if (idle < 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "idle = %i", idle);

	/* wait 5 seconds */
	egg_test_loop_wait (test, 5000);

	egg_test_title (test, "get idle at idle");
	idle = pk_engine_get_seconds_idle (engine);
	if (idle < 6 && idle > 4)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "idle = %i", idle);

	egg_test_title (test, "get idle after method");
	pk_engine_get_daemon_state (engine, &state, NULL);
	g_free (state);
	idle = pk_engine_get_seconds_idle (engine);
	if (idle < 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "idle = %i", idle);

	/************************************************************
	 **********        TEST PROXY NOTIFY              ***********
	 ************************************************************/
	egg_test_title (test, "force test notify updates-changed");
	g_timeout_add (25, (GSourceFunc) pk_test_emit_updates_changed_cb, test);
	egg_test_success (test, NULL);
	egg_test_loop_wait (test, 50);
	egg_test_loop_check (test);

	egg_test_title (test, "force test notify repo-list-changed");
	g_timeout_add (25, (GSourceFunc) pk_test_emit_repo_list_changed_cb, test);
	egg_test_success (test, NULL);
	egg_test_loop_wait (test, 50);
	egg_test_loop_check (test);

	egg_test_title (test, "force test notify wait updates-changed");
	pk_notify_wait_updates_changed (notify, 500);
	egg_test_loop_wait (test, 1000);
	elapsed = egg_test_elapsed (test);
	if (elapsed > 400 && elapsed < 600)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to quit (%i)", elapsed);

	/************************************************************
	 **********               LOCKING                 ***********
	 ************************************************************/
	egg_test_title (test, "test locked");
	inhibit = pk_inhibit_new ();
	pk_inhibit_add (inhibit, GUINT_TO_POINTER (999));
	if (_locked)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "not locked");

	egg_test_title (test, "test locked");
	pk_inhibit_remove (inhibit, GUINT_TO_POINTER (999));
	if (!_locked)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "not locked");
	g_object_unref (inhibit);

	egg_test_title (test, "test not locked");
	if (!_locked)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "still locked");

	/************************************************************
	 **********          BINARY UPGRADE TEST          ***********
	 ************************************************************/
	egg_test_title_assert (test, "restart_schedule not set", !_restart_schedule);
	ret = g_file_set_contents (SBINDIR "/packagekitd", "overwrite", -1, NULL);

	egg_test_title_assert (test, "touched binary file", ret);
	egg_test_loop_wait (test, 5000);

	egg_test_title (test, "get idle after we touched the binary");
	idle = pk_engine_get_seconds_idle (engine);
	if (idle == G_MAXUINT)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "idle = %i", idle);

	egg_test_title_assert (test, "restart_schedule set", _restart_schedule);

	/************************************************************
	 **********             DAEMON QUIT               ***********
	 ************************************************************/
	egg_test_title_assert (test, "not already quit", !_quit);
	egg_test_title (test, "suggest quit with no transactions (should get quit signal)");
	pk_engine_suggest_daemon_quit (engine, NULL);
	if (_quit)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not quit");

	g_object_unref (backend);
	g_object_unref (notify);
	g_object_unref (engine);
}


static void
pk_test_file_monitor_func (EggTest *test)
{
	PkFileMonitor *file_monitor;

	egg_test_title (test, "get a file_monitor");
	file_monitor = pk_file_monitor_new ();
	egg_test_assert (test, file_monitor != NULL);
	g_object_unref (file_monitor);
}


static void
pk_test_inhibit_func (EggTest *test)
{
	PkInhibit *inhibit;
	gboolean ret;

	egg_test_title (test, "get an instance");
	inhibit = pk_inhibit_new ();
	if (inhibit != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, NULL);

	egg_test_title (test, "check we have a connection");
	if (inhibit->priv->proxy != NULL)
		egg_test_success (test, "got proxy");
	else
		egg_test_failed (test, "could not get proxy");

	egg_test_title (test, "check we are not inhibited");
	ret = pk_inhibit_locked (inhibit);
	if (ret == FALSE)
		egg_test_success (test, "marked correctly");
	else
		egg_test_failed (test, "not marked correctly");

	egg_test_title (test, "add 123");
	ret = pk_inhibit_add (inhibit, GUINT_TO_POINTER (123));
	if (ret)
		egg_test_success (test, "inhibited");
	else
		egg_test_failed (test, "could not inhibit");

	egg_test_title (test, "check we are inhibited");
	ret = pk_inhibit_locked (inhibit);
	if (ret)
		egg_test_success (test, "marked correctly");
	else
		egg_test_failed (test, "not marked correctly");

	egg_test_title (test, "add 123 (again)");
	ret = pk_inhibit_add (inhibit, GUINT_TO_POINTER (123));
	if (ret == FALSE)
		egg_test_success (test, "correctly ignored second");
	else
		egg_test_failed (test, "added the same number twice");

	egg_test_title (test, "add 456");
	ret = pk_inhibit_add (inhibit, GUINT_TO_POINTER (456));
	if (ret)
		egg_test_success (test, "inhibited");
	else
		egg_test_failed (test, "could not inhibit");

	egg_test_title (test, "remove 123");
	ret = pk_inhibit_remove (inhibit, GUINT_TO_POINTER (123));
	if (ret)
		egg_test_success (test, "removed first inhibit");
	else
		egg_test_failed (test, "could not remove inhibit");

	egg_test_title (test, "check we are still inhibited");
	ret = pk_inhibit_locked (inhibit);
	if (ret)
		egg_test_success (test, "marked correctly");
	else
		egg_test_failed (test, "not marked correctly");

	egg_test_title (test, "remove 456");
	ret = pk_inhibit_remove (inhibit, GUINT_TO_POINTER (456));
	if (ret)
		egg_test_success (test, "removed second inhibit");
	else
		egg_test_failed (test, "could not remove inhibit");

	egg_test_title (test, "check we are not inhibited");
	ret = pk_inhibit_locked (inhibit);
	if (ret == FALSE)
		egg_test_success (test, "marked correctly");
	else
		egg_test_failed (test, "not marked correctly");

	g_object_unref (inhibit);
}


static void
pk_test_lsof_func (EggTest *test)
{
	gboolean ret;
	PkLsof *lsof;
	GPtrArray *pids;
	gchar *files[] = { "/usr/lib/libssl3.so", NULL };

	egg_test_title (test, "get an instance");
	lsof = pk_lsof_new ();
	egg_test_assert (test, lsof != NULL);

	egg_test_title (test, "refresh lsof data");
	ret = pk_lsof_refresh (lsof);
	egg_test_assert (test, ret);

	egg_test_title (test, "get pids for files");
	pids = pk_lsof_get_pids_for_filenames (lsof, files);
	egg_test_assert (test, pids->len > 0);
	g_ptr_array_unref (pids);

	g_object_unref (lsof);
}


static void
pk_test_notify_func (EggTest *test)
{
	PkNotify *notify;

	egg_test_title (test, "get an instance");
	notify = pk_notify_new ();
	egg_test_assert (test, notify != NULL);

	g_object_unref (notify);
}


static void
pk_test_proc_func (EggTest *test)
{
	gboolean ret;
	PkProc *proc;
//	gchar *files[] = { "/sbin/udevd", NULL };

	egg_test_title (test, "get an instance");
	proc = pk_proc_new ();
	egg_test_assert (test, proc != NULL);

	egg_test_title (test, "refresh proc data");
	ret = pk_proc_refresh (proc);
	egg_test_assert (test, ret);

	g_object_unref (proc);
}


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
new_spawn_object (EggTest *test, PkSpawn **pspawn)
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
	EggTest *test = (EggTest*) data;

	egg_test_title (test, "make sure dispatcher has closed when run idle add");
	if (mexit == PK_SPAWN_EXIT_TYPE_DISPATCHER_EXIT)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "mexit was %i", mexit);

	/* never repeat */
	return FALSE;
}

static void
pk_test_spawn_func (EggTest *test)
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

	/************************************************************
	 **********           Generic tests               ***********
	 ************************************************************/
	egg_test_title (test, "make sure return error for missing file");
	mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
	argv = g_strsplit ("pk-spawn-test-xxx.sh", " ", 0);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_strfreev (argv);
	if (!ret)
		egg_test_success (test, "failed to run invalid file");
	else
		egg_test_failed (test, "ran incorrect file");

	egg_test_title (test, "make sure finished wasn't called");
	if (mexit == PK_SPAWN_EXIT_TYPE_UNKNOWN)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "Called finish for bad file!");

	egg_test_title (test, "make sure run correct helper");
	mexit = -1;
	path = egg_test_get_data_file ("pk-spawn-test.sh");
	argv = g_strsplit (path, " ", 0);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_free (path);
	g_strfreev (argv);
	if (ret)
		egg_test_success (test, "ran correct file");
	else
		egg_test_failed (test, "did not run helper");

	/* wait for finished */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	egg_test_title (test, "make sure finished okay");
	if (mexit == PK_SPAWN_EXIT_TYPE_SUCCESS)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "finish was okay!");

	egg_test_title (test, "make sure finished was called only once");
	if (finished_count == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "finish was called %i times!", finished_count);

	egg_test_title (test, "make sure we got the right stdout data");
	if (stdout_count == 4+11)
		egg_test_success (test, "correct stdout count");
	else
		egg_test_failed (test, "wrong stdout count %i", stdout_count);

	/* get new object */
	new_spawn_object (test, &spawn);

	/************************************************************
	 **********            envp tests                 ***********
	 ************************************************************/
	egg_test_title (test, "make sure we set the proxy");
	mexit = -1;
	path = egg_test_get_data_file ("pk-spawn-proxy.sh");
	argv = g_strsplit (path, " ", 0);
	envp = g_strsplit ("http_proxy=username:password@server:port "
			   "ftp_proxy=username:password@server:port", " ", 0);
	ret = pk_spawn_argv (spawn, argv, envp);
	g_free (path);
	g_strfreev (argv);
	g_strfreev (envp);
	if (ret)
		egg_test_success (test, "ran correct file");
	else
		egg_test_failed (test, "did not run helper");

	/* wait for finished */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	/* get new object */
	new_spawn_object (test, &spawn);

	/************************************************************
	 **********           Killing tests               ***********
	 ************************************************************/
	egg_test_title (test, "make sure run correct helper, and cancel it using SIGKILL");
	mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
	path = egg_test_get_data_file ("pk-spawn-test.sh");
	argv = g_strsplit (path, " ", 0);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_free (path);
	g_strfreev (argv);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not run helper");

	g_timeout_add_seconds (1, cancel_cb, spawn);
	/* wait for finished */
	egg_test_loop_wait (test, 5000);
	egg_test_loop_check (test);

	egg_test_title (test, "make sure finished in SIGKILL");
	if (mexit == PK_SPAWN_EXIT_TYPE_SIGKILL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "finish %i!", mexit);

	/* get new object */
	new_spawn_object (test, &spawn);

	egg_test_title (test, "make sure dumb helper ignores SIGQUIT");
	mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
	path = egg_test_get_data_file ("pk-spawn-test.sh");
	argv = g_strsplit (path, " ", 0);
	g_object_set (spawn,
		      "allow-sigkill", FALSE,
		      NULL);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_free (path);
	g_strfreev (argv);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not run helper");

	g_timeout_add_seconds (1, cancel_cb, spawn);
	/* wait for finished */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	egg_test_title (test, "make sure finished in SIGQUIT");
	if (mexit == PK_SPAWN_EXIT_TYPE_SIGQUIT)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "finish %i!", mexit);

	/* get new object */
	new_spawn_object (test, &spawn);

	egg_test_title (test, "make sure run correct helper, and SIGQUIT it");
	mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
	path = egg_test_get_data_file ("pk-spawn-test-sigquit.sh");
	argv = g_strsplit (path, " ", 0);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_free (path);
	g_strfreev (argv);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not run helper");

	g_timeout_add_seconds (1, cancel_cb, spawn);
	/* wait for finished */
	egg_test_loop_wait (test, 2000);
	egg_test_loop_check (test);

	egg_test_title (test, "make sure finished in SIGQUIT");
	if (mexit == PK_SPAWN_EXIT_TYPE_SIGQUIT)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "finish %i!", mexit);

	egg_test_title (test, "run lots of data for profiling");
	path = egg_test_get_data_file ("pk-spawn-test-profiling.sh");
	argv = g_strsplit (path, " ", 0);
	ret = pk_spawn_argv (spawn, argv, NULL);
	g_free (path);
	g_strfreev (argv);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not run profiling helper");

	/* get new object */
	new_spawn_object (test, &spawn);

	/************************************************************
	 **********  Can we send commands to a dispatcher ***********
	 ************************************************************/
	egg_test_title (test, "run the dispatcher");
	mexit = PK_SPAWN_EXIT_TYPE_UNKNOWN;
	file = egg_test_get_data_file ("pk-spawn-dispatcher.py");
	path = g_strdup_printf ("%s\tsearch-name\tnone\tpower manager", file);
	argv = g_strsplit (path, "\t", 0);
	envp = g_strsplit ("NETWORK=TRUE LANG=C BACKGROUND=TRUE", " ", 0);
	ret = pk_spawn_argv (spawn, argv, envp);
	g_free (file);
	g_free (path);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not run dispatcher");

	egg_test_title (test, "wait 2+2 seconds for the dispatcher");
	/* wait 2 seconds, and make sure we are still running */
	egg_test_loop_wait (test, 4000);
	elapsed = egg_test_elapsed (test);
	if (elapsed > 3900 && elapsed < 4100)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "dispatcher exited");

	egg_test_title (test, "we got a package (+finished)?");
	if (stdout_count == 2)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not get a package");

	egg_test_title (test, "dispatcher still alive?");
	if (spawn->priv->stdin_fd != -1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "dispatcher no longer alive");

	egg_test_title (test, "run the dispatcher with new input");
	ret = pk_spawn_argv (spawn, argv, envp);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not run dispatcher with new input");

	/* this may take a while */
	egg_test_loop_wait (test, 100);

	egg_test_title (test, "we got another package (not finished after bugfix)?");
	if (stdout_count == 3)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not get a package: count was %i", stdout_count);

	/* see if pk_spawn_exit blocks (required) */
	g_idle_add (idle_cb, test);

	egg_test_title (test, "ask dispatcher to close");
	ret = pk_spawn_exit (spawn);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to close dispatcher");

	egg_test_title (test, "ask dispatcher to close (again, should be closing)");
	ret = pk_spawn_exit (spawn);
	if (!ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "attempted to close twice");

	/* this may take a while */
	egg_test_loop_wait (test, 100);

	egg_test_title (test, "did dispatcher close?");
	if (spawn->priv->stdin_fd == -1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "dispatcher still running");

	egg_test_title (test, "did we get the right exit code");
	if (mexit == PK_SPAWN_EXIT_TYPE_DISPATCHER_EXIT)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "finish %i!", mexit);

	egg_test_title (test, "ask dispatcher to close (again)");
	ret = pk_spawn_exit (spawn);
	if (!ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "dispatcher closed twice");

	g_strfreev (argv);
	g_strfreev (envp);
	g_object_unref (spawn);
}


static void
pk_test_store_func (EggTest *test)
{
	PkStore *store;
	gboolean ret;
	const gchar *data_string;
	guint data_uint;
	gboolean data_bool;

	egg_test_title (test, "get an store");
	store = pk_store_new ();
	if (store != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, NULL);

	egg_test_title (test, "set a blank string");
	ret = pk_store_set_string (store, "dave2", "");
	egg_test_assert (test, ret);

	egg_test_title (test, "set a ~bool");
	ret = pk_store_set_bool (store, "roger2", FALSE);
	egg_test_assert (test, ret);

	egg_test_title (test, "set a zero uint");
	ret = pk_store_set_uint (store, "linda2", 0);
	egg_test_assert (test, ret);

	egg_test_title (test, "get a blank string");
	data_string = pk_store_get_string (store, "dave2");
	if (g_strcmp0 (data_string, "") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "data was %s", data_string);

	egg_test_title (test, "get a ~bool");
	data_bool = pk_store_get_bool (store, "roger2");
	if (!data_bool)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "data was %i", data_bool);

	egg_test_title (test, "get a zero uint");
	data_uint = pk_store_get_uint (store, "linda2");
	if (data_uint == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "data was %i", data_uint);

	egg_test_title (test, "set a string");
	ret = pk_store_set_string (store, "dave", "ania");
	egg_test_assert (test, ret);

	egg_test_title (test, "set a bool");
	ret = pk_store_set_bool (store, "roger", TRUE);
	egg_test_assert (test, ret);

	egg_test_title (test, "set a uint");
	ret = pk_store_set_uint (store, "linda", 999);
	egg_test_assert (test, ret);

	egg_test_title (test, "get a string");
	data_string = pk_store_get_string (store, "dave");
	if (g_strcmp0 (data_string, "ania") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "data was %s", data_string);

	egg_test_title (test, "get a bool");
	data_bool = pk_store_get_bool (store, "roger");
	if (data_bool)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "data was %i", data_bool);

	egg_test_title (test, "get a uint");
	data_uint = pk_store_get_uint (store, "linda");
	if (data_uint == 999)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "data was %i", data_uint);

	g_object_unref (store);
}


static void
pk_test_syslog_func (EggTest *test)
{
	PkSyslog *self;

	egg_test_title (test, "get an instance");
	self = pk_syslog_new ();
	if (self != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, NULL);

	g_object_unref (self);
}


static void
pk_test_time_func (EggTest *test)
{
	PkTime *self = NULL;
	gboolean ret;
	guint value;

	egg_test_title (test, "get PkTime object");
	self = pk_time_new ();
	egg_test_assert (test, self != NULL);

	egg_test_title (test, "get elapsed correctly at startup");
	value = pk_time_get_elapsed (self);
	if (value < 10)
		egg_test_success (test, "elapsed at startup %i", value);
	else
		egg_test_failed (test, "elapsed at startup %i", value);

	egg_test_title (test, "ignore remaining correctly");
	value = pk_time_get_remaining (self);
	if (value == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "got %i, not zero!", value);

	g_usleep (1000*1000);

	egg_test_title (test, "get elapsed correctly");
	value = pk_time_get_elapsed (self);
	if (value > 900 && value < 1100)
		egg_test_success (test, "elapsed ~1000ms: %i", value);
	else
		egg_test_failed (test, "elapsed not ~1000ms: %i", value);

	egg_test_title (test, "ignore remaining correctly when not enough entries");
	value = pk_time_get_remaining (self);
	if (value == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "got %i, not zero!", value);

	egg_test_title (test, "make sure we can add data");
	ret = pk_time_add_data (self, 10);
	egg_test_assert (test, ret);

	egg_test_title (test, "make sure we can get remaining correctly");
	value = 20;
	while (value < 60) {
		self->priv->time_offset += 2000;
		pk_time_add_data (self, value);
		value += 10;
	}
	value = pk_time_get_remaining (self);
	if (value > 9 && value < 11)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "got %i", value);

	/* reset */
	g_object_unref (self);
	self = pk_time_new ();

	egg_test_title (test, "make sure we can do long times");
	value = 10;
	pk_time_add_data (self, 0);
	while (value < 60) {
		self->priv->time_offset += 4*60*1000;
		pk_time_add_data (self, value);
		value += 10;
	}
	value = pk_time_get_remaining (self);
	if (value >= 1199 && value <= 1201)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "got %i", value);

	g_object_unref (self);
}


static void
pk_test_transaction_func (EggTest *test)
{
	PkTransaction *transaction = NULL;
	gboolean ret;
	const gchar *temp;
	GError *error = NULL;
#ifdef USE_SECURITY_POLKIT
	const gchar *action;
#endif

	egg_test_title (test, "get PkTransaction object");
	transaction = pk_transaction_new ();
	egg_test_assert (test, transaction != NULL);

	/************************************************************
	 ****************         MAP ROLES        ******************
	 ************************************************************/
#ifdef USE_SECURITY_POLKIT
	egg_test_title (test, "map valid role to action");
	action = pk_transaction_role_to_action_only_trusted (PK_ROLE_ENUM_UPDATE_PACKAGES);
	if (g_strcmp0 (action, "org.freedesktop.packagekit.system-update") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not get correct action '%s'", action);

	egg_test_title (test, "map invalid role to action");
	action = pk_transaction_role_to_action_only_trusted (PK_ROLE_ENUM_SEARCH_NAME);
	if (action == NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not get correct action '%s'", action);
#endif

	/************************************************************
	 ****************          FILTERS         ******************
	 ************************************************************/
	temp = NULL;
	egg_test_title (test, "test a fail filter (null)");
	ret = pk_transaction_filter_check (temp, &error);
	egg_test_assert (test, !ret);
	g_clear_error (&error);

	temp = "";
	egg_test_title (test, "test a fail filter ()");
	ret = pk_transaction_filter_check (temp, &error);
	egg_test_assert (test, !ret);
	g_clear_error (&error);

	temp = ";";
	egg_test_title (test, "test a fail filter (;)");
	ret = pk_transaction_filter_check (temp, &error);
	egg_test_assert (test, !ret);
	g_clear_error (&error);

	temp = "moo";
	egg_test_title (test, "test a fail filter (invalid)");
	ret = pk_transaction_filter_check (temp, &error);
	egg_test_assert (test, !ret);

	g_clear_error (&error);

	temp = "moo;foo";
	egg_test_title (test, "test a fail filter (invalid, multiple)");
	ret = pk_transaction_filter_check (temp, &error);
	egg_test_assert (test, !ret);
	g_clear_error (&error);

	temp = "gui;;";
	egg_test_title (test, "test a fail filter (valid then zero length)");
	ret = pk_transaction_filter_check (temp, &error);
	egg_test_assert (test, !ret);
	g_clear_error (&error);

	temp = "none";
	egg_test_title (test, "test a pass filter (none)");
	ret = pk_transaction_filter_check (temp, &error);
	egg_test_assert (test, ret);
	g_clear_error (&error);

	temp = "gui";
	egg_test_title (test, "test a pass filter (single)");
	ret = pk_transaction_filter_check (temp, &error);
	egg_test_assert (test, ret);
	g_clear_error (&error);

	temp = "devel;~gui";
	egg_test_title (test, "test a pass filter (multiple)");
	ret = pk_transaction_filter_check (temp, &error);
	egg_test_assert (test, ret);
	g_clear_error (&error);

	temp = "~gui;~installed";
	egg_test_title (test, "test a pass filter (multiple2)");
	ret = pk_transaction_filter_check (temp, &error);
	egg_test_assert (test, ret);
	g_clear_error (&error);

	/************************************************************
	 ****************        validate text         **************
	 ************************************************************/
	egg_test_title (test, "validate correct char 1");
	ret = pk_transaction_strvalidate_char ('a');
	egg_test_assert (test, ret);

	egg_test_title (test, "validate correct char 2");
	ret = pk_transaction_strvalidate_char ('~');
	egg_test_assert (test, ret);

	egg_test_title (test, "validate incorrect char");
	ret = pk_transaction_strvalidate_char ('$');
	egg_test_assert (test, !ret);

	egg_test_title (test, "validate incorrect text");
	ret = pk_transaction_strvalidate ("richard$hughes");
	egg_test_assert (test, !ret);

	egg_test_title (test, "validate correct text");
	ret = pk_transaction_strvalidate ("richardhughes");
	egg_test_assert (test, ret);

	g_object_unref (transaction);
}


static void
pk_test_transaction_db_func (EggTest *test)
{
	PkTransactionDb *db;
	guint value;
	gchar *tid;
	gboolean ret;
	guint ms;
	gchar *proxy_http = NULL;
	gchar *proxy_ftp = NULL;
	gchar *root = NULL;
	guint seconds;

	/* remove the self check file */
#if PK_BUILD_LOCAL
	ret = g_file_test (PK_TRANSACTION_DB_FILE, G_FILE_TEST_EXISTS);
	if (ret) {
		egg_test_title (test, "remove old local database");
		egg_warning ("Removing %s", PK_TRANSACTION_DB_FILE);
		value = g_unlink (PK_TRANSACTION_DB_FILE);
		egg_test_assert (test, (value == 0));
	}
#endif

	egg_test_title (test, "check we created quickly");
	db = pk_transaction_db_new ();
	ms = egg_test_elapsed (test);
	if (ms < 1500)
		egg_test_success (test, "acceptable time %ims", ms);
	else
		egg_test_failed (test, "took a long time: %ims", ms);
	g_object_unref (db);

	egg_test_title (test, "check we opened quickly");
	db = pk_transaction_db_new ();
	ms = egg_test_elapsed (test);
	if (ms < 100)
		egg_test_success (test, "acceptable time %ims", ms);
	else
		egg_test_failed (test, "took a long time: %ims", ms);

	egg_test_title (test, "do we get the correct time on a blank database");
	value = pk_transaction_db_action_time_since (db, PK_ROLE_ENUM_REFRESH_CACHE);
	if (value == G_MAXUINT)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get correct time, got %i", value);

	/************************************************************
	 ****************          IDENT           ******************
	 ************************************************************/
	egg_test_title (test, "get an tid object");
	tid = pk_transaction_db_generate_id (db);
	ms = egg_test_elapsed (test);
	if (ms < 10)
		egg_test_success (test, "acceptable time %ims", ms);
	else
		egg_test_failed (test, "took a long time: %ims", ms);
	g_free (tid);

	egg_test_title (test, "get an tid object (no wait)");
	tid = pk_transaction_db_generate_id (db);
	ms = egg_test_elapsed (test);
	if (ms < 5)
		egg_test_success (test, "acceptable time %ims", ms);
	else
		egg_test_failed (test, "took a long time: %ims", ms);
	g_free (tid);

	egg_test_title (test, "set the correct time");
	ret = pk_transaction_db_action_time_reset (db, PK_ROLE_ENUM_REFRESH_CACHE);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to reset value");

	egg_test_title (test, "do the deferred write");
	while (g_main_context_pending (NULL))
		g_main_context_iteration (NULL, TRUE);
	ms = egg_test_elapsed (test);
	if (ms > 1)
		egg_test_success (test, "acceptable time %ims", ms);
	else
		egg_test_failed (test, "took too short time: %ims", ms);

	g_usleep (2*1000*1000);

	egg_test_title (test, "do we get the correct time");
	value = pk_transaction_db_action_time_since (db, PK_ROLE_ENUM_REFRESH_CACHE);
	if (value > 1 && value <= 4)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get correct time, %u", value);

	/************************************************************
	 ****************          PROXIES         ******************
	 ************************************************************/
	egg_test_title (test, "can we set the proxies");
	ret = pk_transaction_db_set_proxy (db, 500, "session1", "127.0.0.1:80", "127.0.0.1:21");
	egg_test_assert (test, ret);

	egg_test_title (test, "can we set the proxies (overwrite)");
	ret = pk_transaction_db_set_proxy (db, 500, "session1", "127.0.0.1:8000", "127.0.0.1:21");
	egg_test_assert (test, ret);

	egg_test_title (test, "can we get the proxies (non-existant user)");
	ret = pk_transaction_db_get_proxy (db, 501, "session1", &proxy_http, &proxy_ftp);
	if (proxy_http == NULL && proxy_ftp == NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get correct proxies, %s and %s", proxy_http, proxy_ftp);

	egg_test_title (test, "can we get the proxies (non-existant session)");
	ret = pk_transaction_db_get_proxy (db, 500, "session2", &proxy_http, &proxy_ftp);
	if (proxy_http == NULL && proxy_ftp == NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get correct proxies, %s and %s", proxy_http, proxy_ftp);

	egg_test_title (test, "can we get the proxies (match)");
	ret = pk_transaction_db_get_proxy (db, 500, "session1", &proxy_http, &proxy_ftp);
	if (g_strcmp0 (proxy_http, "127.0.0.1:8000") == 0 &&
	    g_strcmp0 (proxy_ftp, "127.0.0.1:21") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get correct proxies, %s and %s", proxy_http, proxy_ftp);

	/************************************************************
	 ****************            ROOT          ******************
	 ************************************************************/
	egg_test_title (test, "can we set the root");
	ret = pk_transaction_db_set_root (db, 500, "session1", "/mnt/chroot");
	egg_test_assert (test, ret);

	egg_test_title (test, "can we set the root (overwrite)");
	ret = pk_transaction_db_set_root (db, 500, "session1", "/mnt/chroot2");
	egg_test_assert (test, ret);

	egg_test_title (test, "can we get the root (non-existant user)");
	ret = pk_transaction_db_get_root (db, 501, "session1", &root);
	if (root == NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get correct root: %s", root);

	egg_test_title (test, "can we get the root (match)");
	ret = pk_transaction_db_get_root (db, 500, "session1", &root);
	if (g_strcmp0 (root, "/mnt/chroot2") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get correct root: %s", root);

	g_free (root);
	g_free (proxy_http);
	g_free (proxy_ftp);
	g_object_unref (db);}


static void
pk_test_extra_trans_func (EggTest *test)
{
	PkTransactionExtra *extra;

	egg_test_title (test, "get an instance");
	extra = pk_transaction_extra_new ();
	egg_test_assert (test, extra != NULL);

	g_object_unref (extra);
}


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
pk_test_transaction_list_func (EggTest *test)
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
		egg_test_title (test, "remove old local database");
		egg_warning ("Removing %s", "./transactions.db");
		size = g_unlink ("./transactions.db");
		egg_test_assert (test, (size == 0));
	}
#endif

	/* we get a cache object to reproduce the engine having it ref'd */
	cache = pk_cache_new ();
	db = pk_transaction_db_new ();

	egg_test_title (test, "get a transaction list object");
	tlist = pk_transaction_list_new ();
	egg_test_assert (test, tlist != NULL);

	egg_test_title (test, "make sure we get a valid tid");
	tid = pk_transaction_db_generate_id (db);
	if (tid != NULL)
		egg_test_success (test, "got tid %s", tid);
	else
		egg_test_failed (test, "failed to get tid");

	egg_test_title (test, "create a transaction object");
	ret = pk_transaction_list_create (tlist, tid, ":0", NULL);
	if (ret)
		egg_test_success (test, "created transaction %s", tid);
	else
		egg_test_failed (test, "failed to create transaction");

	egg_test_title (test, "make sure we get the right object back");
	item = pk_transaction_list_get_from_tid (tlist, tid);
	if (item != NULL &&
	    (g_strcmp0 (item->tid, tid) == 0) &&
	    item->transaction != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "could not find in db");

	egg_test_title (test, "make sure item has correct flags");
	if (item->running == FALSE && item->committed == FALSE && item->finished == FALSE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item->running, item->committed, item->finished);

	egg_test_title (test, "get size one we have in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	egg_test_title (test, "get transactions (committed, not finished) in progress");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
	g_strfreev (array);

	egg_test_title (test, "add again the same tid (should fail)");
	ret = pk_transaction_list_create (tlist, tid, ":0", NULL);
	if (!ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "added the same tid twice");

	egg_test_title (test, "remove without ever committing");
	ret = pk_transaction_list_remove (tlist, tid);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to remove");

	egg_test_title (test, "get size none we have in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	/* get a new tid */
	g_free (tid);
	tid = pk_transaction_db_generate_id (db);

	egg_test_title (test, "create another item");
	ret = pk_transaction_list_create (tlist, tid, ":0", NULL);
	if (ret)
		egg_test_success (test, "created transaction %s", tid);
	else
		egg_test_failed (test, "failed to create transaction");

	PkBackend *backend;
	backend = pk_backend_new ();
	egg_test_title (test, "try to load a valid backend");
	ret = pk_backend_set_name (backend, "dummy");
	egg_test_assert (test, ret);

	egg_test_title (test, "lock an valid backend");
	ret = pk_backend_lock (backend);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to lock");

	egg_test_title (test, "get from db");
	item = pk_transaction_list_get_from_tid (tlist, tid);
	if (item != NULL &&
	    (g_strcmp0 (item->tid, tid) == 0) &&
	    item->transaction != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "could not find in db");

	g_signal_connect (item->transaction, "finished",
			  G_CALLBACK (pk_transaction_list_test_finished_cb), test);

	/* this tests the run-on-commit action */
	pk_transaction_get_updates (item->transaction, "none", NULL);

	egg_test_title (test, "make sure item has correct flags");
	if (item->running == TRUE && item->committed == TRUE && item->finished == FALSE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item->running, item->committed, item->finished);

	egg_test_title (test, "get present role");
	ret = pk_transaction_list_role_present (tlist, PK_ROLE_ENUM_GET_UPDATES);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "did not get role");

	egg_test_title (test, "get non-present role");
	ret = pk_transaction_list_role_present (tlist, PK_ROLE_ENUM_SEARCH_NAME);
	if (!ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "got missing role");

	egg_test_title (test, "get size we have in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	egg_test_title (test, "get transactions (committed, not finished) in progress");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
	g_strfreev (array);

	/* wait for Finished */
	egg_test_loop_wait (test, 2000);
	egg_test_loop_check (test);

	egg_test_title (test, "get size one we have in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	egg_test_title (test, "get transactions (committed, not finished) in progress (none)");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
	g_strfreev (array);

	egg_test_title (test, "remove already removed");
	ret = pk_transaction_list_remove (tlist, tid);
	if (!ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "tried to remove");

	/* wait for Cleanup */
	g_timeout_add_seconds (5, (GSourceFunc) pk_transaction_list_test_delay_cb, test);
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	egg_test_title (test, "make sure queue empty");
	size = pk_transaction_list_get_size (tlist);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	g_free (tid);

	/************************************************************
	 ***************  Get updates from cache    *****************
	 ************************************************************/
	item = pk_transaction_list_test_get_item (tlist);
	g_signal_connect (item->transaction, "finished",
			  G_CALLBACK (pk_transaction_list_test_finished_cb), test);

	pk_transaction_get_updates (item->transaction, "none", NULL);

	/* wait for cached results*/
	egg_test_loop_wait (test, 1000);
	egg_test_loop_check (test);

	egg_test_title (test, "make sure item has correct flags");
	if (item->running == FALSE && item->committed == TRUE && item->finished == TRUE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item->running, item->committed, item->finished);

	egg_test_title (test, "get transactions (committed, not finished) in progress (none, as cached)");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
	g_strfreev (array);

	egg_test_title (test, "get size we have in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	/* wait for Cleanup */
	g_timeout_add_seconds (5, (GSourceFunc) pk_transaction_list_test_delay_cb, test);
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	egg_test_title (test, "get transactions (committed, not finished) in progress (none, as cached)");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
	g_strfreev (array);

	egg_test_title (test, "get size we have in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	/************************************************************
	 ****************  Chained transactions    ******************
	 ************************************************************/

	/* create three instances in list */
	item1 = pk_transaction_list_test_get_item (tlist);
	item2 = pk_transaction_list_test_get_item (tlist);
	item3 = pk_transaction_list_test_get_item (tlist);

	egg_test_title (test, "get all items in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 3)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	egg_test_title (test, "get transactions (committed, not finished) committed");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
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

	egg_test_title (test, "get transactions (committed, not finished) in progress (all)");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 3)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
	g_strfreev (array);

	/* wait for first action */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	egg_test_title (test, "get all items in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 3)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	egg_test_title (test, "get transactions (committed, not finished) (two, first one finished)");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 2)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
	g_strfreev (array);

	egg_test_title (test, "make sure item1 has correct flags");
	if (item1->running == FALSE && item1->committed == TRUE && item1->finished == TRUE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item1->running, item1->committed, item1->finished);

	egg_test_title (test, "make sure item2 has correct flags");
	if (item2->running == TRUE && item2->committed == TRUE && item2->finished == FALSE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item2->running, item2->committed, item2->finished);

	egg_test_title (test, "make sure item3 has correct flags");
	if (item3->running == FALSE && item3->committed == TRUE && item3->finished == FALSE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item3->running, item3->committed, item3->finished);

	/* wait for second action */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	egg_test_title (test, "get all items in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 3)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	egg_test_title (test, "get transactions (committed, not finished) in progress (one)");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
	g_strfreev (array);

	egg_test_title (test, "make sure item1 has correct flags");
	if (item1->running == FALSE && item1->committed == TRUE && item1->finished == TRUE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item1->running, item1->committed, item1->finished);

	egg_test_title (test, "make sure item2 has correct flags");
	if (item2->running == FALSE && item2->committed == TRUE && item2->finished == TRUE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item2->running, item2->committed, item2->finished);

	egg_test_title (test, "make sure item3 has correct flags");
	if (item3->running == TRUE && item3->committed == TRUE && item3->finished == FALSE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item3->running, item3->committed, item3->finished);

	/* wait for third action */
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	egg_test_title (test, "get all items in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 3)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	egg_test_title (test, "get transactions (committed, not finished) in progress (none)");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
	g_strfreev (array);

	egg_test_title (test, "make sure item1 has correct flags");
	if (item1->running == FALSE && item1->committed == TRUE && item1->finished == TRUE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item1->running, item1->committed, item1->finished);

	egg_test_title (test, "make sure item2 has correct flags");
	if (item2->running == FALSE && item2->committed == TRUE && item2->finished == TRUE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item2->running, item2->committed, item2->finished);

	egg_test_title (test, "make sure item3 has correct flags");
	if (item3->running == FALSE && item3->committed == TRUE && item3->finished == TRUE)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "wrong flags: running[%i] committed[%i] finished[%i]",
				 item3->running, item3->committed, item3->finished);

	/* wait for Cleanup */
	g_timeout_add_seconds (5, (GSourceFunc) pk_transaction_list_test_delay_cb, test);
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	egg_test_title (test, "get both items in queue");
	size = pk_transaction_list_get_size (tlist);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);

	egg_test_title (test, "get transactions (committed, not finished) in progress (neither - again)");
	array = pk_transaction_list_get_array (tlist);
	size = g_strv_length (array);
	if (size == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "size %i", size);
	g_strfreev (array);

	g_object_unref (tlist);
	g_object_unref (backend);
	g_object_unref (cache);
	g_object_unref (db);
}

int
main (int argc, char **argv)
{
	if (! g_thread_supported ())
		g_thread_init (NULL);
	g_type_init ();
	egg_debug_init (&argc, &argv);
	g_test_init (&argc, &argv, NULL);

	/* egg */
//	egg_string_func (test);

	/* components */
	pk_test_proc_func (test);
	pk_test_lsof_func (test);
	pk_test_file_monitor_func (test);
	pk_test_time_func (test);
	pk_test_conf_func (test);
	pk_test_store_func (test);
	pk_test_inhibit_func (test);
	pk_test_spawn_func (test);
	pk_test_transaction_list_func (test);
	pk_test_transaction_db_func (test);

	/* backend stuff */
	pk_test_backend_func (test);
	pk_test_backend_spawn_func (test);

	/* system */
	pk_test_engine_func (test);

	return g_test_run ();
}


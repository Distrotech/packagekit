helperdir = $(datadir)/PackageKit/helpers/click
dist_helper_DATA = 			\
	clickBackend.py			\
	$(NULL)

plugindir = $(PK_PLUGIN_DIR)
plugin_LTLIBRARIES = libpk_backend_click.la
libpk_backend_click_la_SOURCES = pk-backend-click.c
libpk_backend_click_la_LIBADD = $(PK_PLUGIN_LIBS)
libpk_backend_click_la_LDFLAGS = -module -avoid-version
libpk_backend_click_la_CFLAGS = $(PK_PLUGIN_CFLAGS) $(WARNINGFLAGS_C)

install-data-hook:
	chmod a+rx $(DESTDIR)$(helperdir)/*.py

clean-local:
	rm -f *.pyc
	rm -f *~


-include $(top_srcdir)/git.mk

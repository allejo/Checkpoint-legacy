lib_LTLIBRARIES = Checkpoint.la

Checkpoint_la_SOURCES = Checkpoint.cpp
Checkpoint_la_CPPFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/plugins/plugin_utils
Checkpoint_la_LDFLAGS = -module -avoid-version -shared
Checkpoint_la_LIBADD = $(top_builddir)/plugins/plugin_utils/libplugin_utils.la

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

EXTRA_DIST = \
	LICENSE.md \
	README.md \
	Checkpoint.def \
	Checkpoint.sln \
	Checkpoint.vcxproj \
	Checkpoint.vcxproj.filters

MAINTAINERCLEANFILES =	\
	Makefile.in

## Process this file with automake to produce Makefile.in
# Makefile.am for enfle
# Last Modified: Mon Nov 22 23:52:43 2004.
# $Id$

STATIC_PLUGINS = @Static_plugins@
MORE_CFLAGS = $(shell if echo $(STATIC_PLUGINS) | grep $(PLUGIN_ID) >/dev/null; then echo -n "-DSTATIC -static"; else echo -n ""; fi)
MORE_LDFLAGS = $(shell if echo $(STATIC_PLUGINS) | grep $(PLUGIN_ID) >/dev/null; then echo -n "-static"; else echo -n "-avoid-version"; fi)

PLUGIN_ID = archiver/7z
ENFLE_DIR = @Enfle_dir@
libdir = $(ENFLE_DIR)/lib/enfle/plugins/archiver

if BUILD_ENFLE
lib_LTLIBRARIES = archiver_7z.la
endif
ENFLE_SRCDIR = @Enfle_srcdir@
INCLUDES = -I../include -I$(ENFLE_SRCDIR) -I$(ENFLE_SRCDIR)/lib -I$(ENFLE_SRCDIR)/lib/enfle

archiver_7z_la_LIBADD = ../lib/lib7z.la
archiver_7z_la_LDFLAGS = $(MORE_LDFLAGS) -module
archiver_7z_la_CFLAGS = $(MORE_CFLAGS)
noinst_HEADERS = enflestream.h
archiver_7z_la_SOURCES = 7z.c enflestream.c

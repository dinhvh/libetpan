# libEtPan! -- a mail stuff library

include $(top_srcdir)/rules.mk

etpaninclude_HEADERS = \
	mailactivesync.h mailactivesync_types.h

AM_CPPFLAGS = -I$(top_builddir)/include \
	-I$(top_srcdir)/src/data-types

noinst_LTLIBRARIES = libactivesync.la

libactivesync_la_SOURCES = \
	mailactivesync.c mailactivesync_types.c

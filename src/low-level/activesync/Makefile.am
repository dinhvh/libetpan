# libEtPan! -- a mail stuff library

include $(top_srcdir)/rules.mk

etpaninclude_HEADERS = \
	mailactivesync.h mailactivesync_codes.h \
	mailactivesync_types.h mailactivesync_wbxml.h

AM_CPPFLAGS = -I$(top_builddir)/include \
	-I$(top_srcdir)/src/data-types

noinst_LTLIBRARIES = libactivesync.la

libactivesync_la_SOURCES = \
	mailactivesync.c mailactivesync_codes.c \
	mailactivesync_types.c mailactivesync_wbxml.c

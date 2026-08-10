# libEtPan! -- a mail stuff library

include $(top_srcdir)/rules.mk

etpaninclude_HEADERS = \
	mailsmime.h mailsmime_types.h

AM_CPPFLAGS = -I$(top_builddir)/include \
	-I$(top_srcdir)/src/data-types \
	-I$(top_srcdir)/src/low-level/imf \
	-I$(top_srcdir)/src/low-level/mime

noinst_LTLIBRARIES = libsmime.la

libsmime_la_SOURCES = \
	mailsmime.c

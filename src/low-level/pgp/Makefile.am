# libEtPan! -- a mail stuff library

include $(top_srcdir)/rules.mk

etpaninclude_HEADERS = \
	mailpgp.h mailpgp_types.h

AM_CPPFLAGS = -I$(top_builddir)/include \
	-I$(top_srcdir)/src/data-types \
	-I$(top_srcdir)/src/low-level/imf \
	-I$(top_srcdir)/src/low-level/mime \
	@RNP_CFLAGS@

noinst_LTLIBRARIES = libpgp.la

libpgp_la_SOURCES = \
	mailpgp.c mailpgp_rnp.c

libpgp_la_LIBADD = @RNP_LIBS@

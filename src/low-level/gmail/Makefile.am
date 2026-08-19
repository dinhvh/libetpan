# libEtPan! -- a mail stuff library

include $(top_srcdir)/rules.mk

etpaninclude_HEADERS = \
	mailgmail.h mailgmail_types.h

noinst_HEADERS = \
	mailgmail_http.h mailgmail_parser.h mailgmail_private.h mailgmail_url.h

AM_CPPFLAGS = -I$(top_builddir)/include \
	-I$(top_srcdir)/src/data-types

# Libtool has no Objective-C language tag; compile the macOS .m source using
# the C configuration instead of relying on compiler-name inference.
AM_LIBTOOLFLAGS = --tag=CC

noinst_LTLIBRARIES = libgmail.la

libgmail_la_SOURCES = \
	mailgmail.c mailgmail_types.c \
	mailgmail_http.c mailgmail_parser.c mailgmail_url.c

libgmail_la_LINK = $(LINK)

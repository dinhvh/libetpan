# libEtPan! -- a mail stuff library

include $(top_srcdir)/rules.mk

etpaninclude_HEADERS = \
	mailgmail.h mailgmail_types.h

noinst_HEADERS = \
	mailgmail_http.h mailgmail_parser.h mailgmail_private.h mailgmail_url.h

AM_CPPFLAGS = -I$(top_builddir)/include \
	-I$(top_srcdir)/src/data-types

noinst_LTLIBRARIES = libgmail.la

if HAVE_NSURLSESSION
nsurlsession_sources = mailgmail_http_nsurlsession.m
else
nsurlsession_sources =
endif

libgmail_la_SOURCES = \
	mailgmail.c mailgmail_types.c \
	mailgmail_http.c mailgmail_http_curl.c \
	mailgmail_parser.c mailgmail_url.c \
	$(nsurlsession_sources)

libgmail_la_LINK = $(LINK)

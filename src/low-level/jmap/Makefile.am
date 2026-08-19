# libEtPan! -- a mail stuff library

include $(top_srcdir)/rules.mk

etpaninclude_HEADERS = \
	mailjmap.h mailjmap_blob.h mailjmap_http.h mailjmap_mail.h \
	mailjmap_request.h mailjmap_response.h mailjmap_types.h

etpaninclude_CONDITION = defined(HAVE_JMAP) || (defined(LIBETPAN_HAS_JMAP) && LIBETPAN_HAS_JMAP)

noinst_HEADERS = mailjmap_private.h

AM_CPPFLAGS = -I$(top_builddir)/include \
	-I$(top_srcdir)/src/data-types

noinst_LTLIBRARIES = libjmap.la

libjmap_la_SOURCES = \
	mailjmap.c mailjmap_blob.c mailjmap_http.c \
	mailjmap_mail.c \
	mailjmap_request.c mailjmap_response.c \
	mailjmap_types.c

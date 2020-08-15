# libEtPan! -- a mail stuff library
#
# Copyright (C) 2007 g10 Code GmbH
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the libEtPan! project nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

include $(top_srcdir)/rules.mk

EXTRA_DIST = TODO

etpaninclude_HEADERS = \
	mailimap_helper.h mailimap.h \
	mailimap_types.h mailimap_types_helper.h \
	mailimap_socket.h mailimap_ssl.h \
	mailimap_extension.h mailimap_extension_types.h \
	annotatemore.h annotatemore_types.h \
	acl.h acl_types.h \
	uidplus.h uidplus_types.h \
	quota.h quota_parser.h quota_sender.h quota_types.h \
	idle.h \
	namespace.h namespace_parser.h namespace_sender.h namespace_types.h \
	xlist.h xgmlabels.h xgmmsgid.h xgmthrid.h \
	mailimap_id.h mailimap_id_types.h \
	enable.h condstore.h condstore_types.h \
	qresync.h qresync_types.h \
	mailimap_sort.h mailimap_sort_types.h \
  mailimap_compress.h \
  mailimap_oauth2.h \
  clientid.h

AM_CPPFLAGS = -I$(top_builddir)/include \
	-I$(top_srcdir)/src/data-types

noinst_LTLIBRARIES = libimap.la

libimap_la_SOURCES = \
	mailimap_types.c mailimap_types_helper.c \
	mailimap_sender.h mailimap_sender.c \
	mailimap_keywords.h mailimap_keywords.c \
	mailimap_parser.h mailimap_parser.c \
        mailimap.c \
        mailimap_helper.c \
        mailimap_socket.c \
        mailimap_ssl.c \
	mailimap_print.h mailimap_print.c \
	mailimap_extension.c \
	annotatemore.c \
	annotatemore_parser.h annotatemore_parser.c \
	annotatemore_sender.h annotatemore_sender.c \
	annotatemore_types.c \
	acl.c \
	acl_parser.h acl_parser.c \
	acl_sender.h acl_sender.c \
	acl_types.c \
	uidplus.c \
	uidplus_sender.h uidplus_sender.c \
	uidplus_types.c \
	uidplus_parser.h uidplus_parser.c \
	idle.c idle.h\
	quota.c quota.h \
	quota_parser.c quota_parser.h \
	quota_sender.c quota_sender.h \
	quota_types.c quota_types.h \
	namespace.c namespace.h \
	namespace_parser.c namespace_parser.h \
	namespace_sender.c namespace_sender.h \
	namespace_types.c namespace_types.h \
	xlist.c xlist.h \
	xgmlabels.c xgmlabels.h \
	xgmmsgid.c xgmmsgid.h \
	xgmthrid.c xgmthrid.h \
	mailimap_id.h mailimap_id.c \
	mailimap_id_types.h mailimap_id_types.c \
	mailimap_id_sender.h mailimap_id_sender.c \
	mailimap_id_parser.h mailimap_id_parser.c \
	enable.h enable.c \
	condstore.h condstore.c condstore_types.h condstore_types.c condstore_private.h \
	qresync.h qresync.c qresync_types.h qresync_types.c qresync_private.h \
	mailimap_sort.c mailimap_sort.h \
  mailimap_sort_types.c mailimap_sort_types.h \
  mailimap_compress.c mailimap_compress.h \
  mailimap_oauth2.c mailimap_oauth2.h \
  clientid_sender.c clientid_sender.h \
  clientid.c clientid.h

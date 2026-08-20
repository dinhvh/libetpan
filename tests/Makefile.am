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

EXTRA_DIST = README activesync-ms-device-oauth.py activesync-live-sync-test.sh \
	fastmail-jmap-oauth.py gmail-google-oauth.py gmail-live-smoke-test.sh \
	jmap-live-smoke-test.sh \
	smime-openssl-fixture-test.sh \
	mailpgp-gnupg-fixture-test.sh \
	fuzz/README.md \
	fuzz/fuzz_imap_response.c \
	fuzz/fuzz_jmap_response.c \
	fuzz/imap.dict \
	fuzz/jmap.dict \
	fuzz/seeds/01-server-greeting.imap \
	fuzz/seeds/02-tagged-ok.imap \
	fuzz/seeds/03-exists-recent.imap \
	fuzz/seeds/04-fetch-flags-uid.imap \
	fuzz/seeds/05-fetch-body-structure.imap \
	fuzz/seeds/06-list-response.imap \
	fuzz/seeds/07-status-response.imap \
	fuzz/seeds/08-bye.imap \
	fuzz/seeds/09-permanentflags.imap \
	fuzz/seeds/10-search-response.imap \
	fuzz/jmap-seeds/00e16c19687a5769f5abf23fd38884dbb5d8cb4f \
	fuzz/jmap-seeds/01-method-response.json \
	fuzz/jmap-seeds/02-method-error.json \
	fuzz/jmap-seeds/03-problem-response.json \
	fuzz/jmap-seeds/04-batched-response.json \
	fuzz/jmap-seeds/043d5d94784763ec4f110552d1185687e0f4406f \
	fuzz/jmap-seeds/60ba4b2daa4ed4d070fec06687e249e0e6f9ee45 \
	fuzz/jmap-seeds/e8341a3d4611a65c09a0557157384d5075c05b36

if HAVE_JMAP
jmap_test_programs = jmap-json-test jmap-http-test jmap-request-test \
	jmap-response-test jmap-call-test jmap-mailbox-test \
	jmap-thread-test jmap-email-test jmap-blob-test \
	jmap-session-test
jmap_sample_programs = jmap-sample
jmap_live_tests = jmap-live-smoke-test.sh
else
jmap_test_programs =
jmap_sample_programs =
jmap_live_tests =
endif

gmail_live_tests = gmail-live-smoke-test.sh

noinst_PROGRAMS = smime decrypt pgp frm frm-tree frm-simple	\
	readmsg-simple fetch-attachment smtpsend readmsg-uid \
	readmsg compose-msg imap-sample mime-create mime-parse \
	pop-sample activesync-sample mailhttp-test feed-http-test \
	gmail-http-test gmail-sample \
	mailstream-ssl-backend-selection-test mailstream-ssl-internal-transport-test \
	mailstream-voip-compatibility-test \
	smime-low-level-test smime-openssl-interop-test \
	mailpgp-low-level-test mailpgp-gnupg-interop-test \
	$(jmap_test_programs) $(jmap_sample_programs)

TESTS = mailstream-ssl-backend-selection-test mailstream-ssl-internal-transport-test \
	mailstream-voip-compatibility-test \
	mailhttp-test feed-http-test smime-low-level-test mailpgp-low-level-test \
	smime-openssl-fixture-test.sh mailpgp-gnupg-fixture-test.sh \
	$(jmap_test_programs) $(jmap_live_tests) $(gmail_live_tests)

# For W32, reverse the -DLIBETPAN_DLL.  Unfortunately, CFLAGS comes
# after AM_CPPFLAGS, so we have to frob CFLAGS.
CFLAGS += -ULIBETPAN_DLL
AM_CPPFLAGS = -I$(top_builddir)/include \
	@JSON_C_CFLAGS@ \
	-I$(top_srcdir)/src/driver/interface \
	-I$(top_srcdir)/src/driver/tools \
	-I$(top_srcdir)/src/driver/implementation/data-message \
	-I$(top_srcdir)/src/driver/implementation/imap \
	-I$(top_srcdir)/src/driver/implementation/nntp \
	-I$(top_srcdir)/src/driver/implementation/pop3 \
	-include config.h \
	-include data_message_driver.h \
	-include maildriver.h \
	-include imapdriver_types.h \
	-include nntpdriver_types.h \
	-include pop3driver_types.h
LDADD = ../src/libetpan.la

FRMCOMMON = frm-common.h frm-common.c option-parser.h option-parser.c

frm_SOURCES = $(FRMCOMMON) frm.c

frm_tree_SOURCES = $(FRMCOMMON) frm-tree.c

frm_simple_SOURCES = $(FRMCOMMON) frm-simple.c

READMSGCOMMON = readmsg-common.h readmsg-common.c option-parser.h	\
	option-parser.c

readmsg_SOURCES = $(READMSGCOMMON) readmsg.c

smime_low_level_test_SOURCES = $(top_srcdir)/unittest/smime/smime_low_level_test.c
smime_low_level_test_LDADD = ../src/libetpan.la @SSLLIBS@ @SMIMELIBS@
smime_openssl_interop_test_SOURCES = smime-openssl-interop-test.c
smime_openssl_interop_test_LDADD = ../src/libetpan.la @SSLLIBS@ @SMIMELIBS@
mailpgp_low_level_test_SOURCES = $(top_srcdir)/unittest/mailpgp/mailpgp_low_level_test.c
mailpgp_low_level_test_LDADD = ../src/libetpan.la @RNP_LIBS@
mailpgp_gnupg_interop_test_SOURCES = mailpgp-gnupg-interop-test.c
mailpgp_gnupg_interop_test_LDADD = ../src/libetpan.la @RNP_LIBS@

readmsg_simple_SOURCES = $(READMSGCOMMON) readmsg-simple.c

readmsg_uid_SOURCES = $(READMSGCOMMON) readmsg-uid.c

fetch_attachment_SOURCES = $(READMSGCOMMON) fetch-attachment.c

gmail_http_test_SOURCES = $(top_srcdir)/unittest/gmail/http_test.c

mailhttp_test_SOURCES = $(top_srcdir)/unittest/http/mailhttp_test.c

mailstream_ssl_backend_selection_test_SOURCES = \
	$(top_srcdir)/unittest/mailstream-ssl/backend_selection_test.c

mailstream_ssl_internal_transport_test_SOURCES = \
	$(top_srcdir)/unittest/mailstream-ssl/internal_transport_test.c \
	$(top_srcdir)/src/data-types/mailstream_ssl_internal.c

mailstream_voip_compatibility_test_SOURCES = \
	$(top_srcdir)/unittest/mailstream-voip/compatibility_test.c

feed_http_test_SOURCES = $(top_srcdir)/unittest/feed/feed_http_test.c

gmail_sample_SOURCES = gmail-sample.c

jmap_json_test_SOURCES = $(top_srcdir)/unittest/jmap/jmap-json-test.c

jmap_http_test_SOURCES = $(top_srcdir)/unittest/jmap/jmap-http-test.c

jmap_request_test_SOURCES = $(top_srcdir)/unittest/jmap/jmap-request-test.c

jmap_response_test_SOURCES = $(top_srcdir)/unittest/jmap/jmap-response-test.c

jmap_call_test_SOURCES = $(top_srcdir)/unittest/jmap/jmap-call-test.c

jmap_mailbox_test_SOURCES = $(top_srcdir)/unittest/jmap/jmap-mailbox-test.c

jmap_thread_test_SOURCES = $(top_srcdir)/unittest/jmap/jmap-thread-test.c

jmap_email_test_SOURCES = $(top_srcdir)/unittest/jmap/jmap-email-test.c

jmap_blob_test_SOURCES = $(top_srcdir)/unittest/jmap/jmap-blob-test.c

jmap_session_test_SOURCES = $(top_srcdir)/unittest/jmap/jmap-session-test.c

jmap_sample_SOURCES = jmap-sample.c

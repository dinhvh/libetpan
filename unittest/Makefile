CC ?= cc

TOP = ..

CPPFLAGS += \
	-DHAVE_CONFIG_H \
	-I$(TOP) \
	-I$(TOP)/include \
	-I$(TOP)/src/data-types \
	-I$(TOP)/src/driver/interface \
	-I$(TOP)/src/driver/tools \
	-I$(TOP)/src/driver/implementation/data-message \
	-I$(TOP)/src/driver/implementation/imap \
	-I$(TOP)/src/driver/implementation/nntp \
	-I$(TOP)/src/driver/implementation/pop3 \
	-I$(TOP)/src/low-level/activesync \
	-I$(TOP)/src/low-level/imf \
	-I$(TOP)/src/low-level/imap \
	-I$(TOP)/src/low-level/mime \
	-Icommon

CFLAGS += -Wall -Wextra -Werror -g
LIBETPAN_TEST_DEPENDENCY ?= libetpan
LIBETPAN_TEST_LDLIBS ?= $(TOP)/src/.libs/libetpan.a
LDLIBS += $(LIBETPAN_TEST_LDLIBS) -lz -lcurl -lsasl2 -lpthread
LDLIBS += $(shell pkg-config --libs icu-uc 2>/dev/null)
ifeq ($(shell uname -s),Darwin)
LDLIBS += -framework CoreFoundation -framework Foundation -liconv
endif
JSON_C_LDLIBS := $(shell pkg-config --libs json-c 2>/dev/null || printf '%s' "-ljson-c")
OPENSSL_LDLIBS := $(shell pkg-config --libs openssl 2>/dev/null || printf '%s' "-lssl -lcrypto")
RNP_LDLIBS := $(shell pkg-config --libs rnp 2>/dev/null || printf '%s' "-lrnp")
XML2_CPPFLAGS := $(shell pkg-config --cflags libxml-2.0 2>/dev/null || { command -v xcrun >/dev/null 2>&1 && printf '%s' "-I$$(xcrun --show-sdk-path)/usr/include/libxml2" || printf '%s' "-I/usr/include/libxml2"; })
XML2_LDLIBS := -lxml2
TIDY_CPPFLAGS := $(shell pkg-config --cflags tidy 2>/dev/null || printf '%s' "-I$$(brew --prefix tidy-html5)/include") -I /usr/include/tidy
TIDY_LDLIBS := $(shell pkg-config --libs tidy 2>/dev/null || printf '%s' "-L$$(brew --prefix tidy-html5)/lib -ltidy")
MATH_LDLIBS := -lm
XCODEBUILD ?= xcodebuild

IMAP_TEST = imap/imap_test
IMF_TEST = imf/imf_test
MIME_TEST = mime/mime_test
MIME_BUILDER_TEST = mime-builder/mime_builder_test
MIME_PARSER_SERIALIZATION_TEST = mime-parser/mime_parser_serialization_test
CHARSET_DETECTION_TEST = charset-detection/charset_detection_test
PLAINTEXT_RENDERING_TEST = plaintext-rendering/plaintext_rendering_test
IMAP_UTF7_TEST = imap-utf7/imap_utf7_test
DATA_TYPES_TEST = data-types/data_types_test
CHARCONV_TEST = charconv/charconv_test
MAILHTTP_TEST = http/http_test
FEED_HTTP_TEST = feed/feed_http_test
ACTIVESYNC_WBXML_TEST = activesync/wbxml_test
ACTIVESYNC_HTTP_TEST = activesync/http_test
ACTIVESYNC_SAMPLE_TEST = activesync/sample_test
POP3_TEST = pop3/pop3_test
GMAIL_TEST = gmail/gmail_test
JMAP_JSON_TEST = jmap/jmap_json_test
JMAP_HTTP_TEST = jmap/jmap_http_test
JMAP_REQUEST_TEST = jmap/jmap_request_test
JMAP_RESPONSE_TEST = jmap/jmap_response_test
JMAP_CALL_TEST = jmap/jmap_call_test
JMAP_MAILBOX_TEST = jmap/jmap_mailbox_test
JMAP_THREAD_TEST = jmap/jmap_thread_test
JMAP_EMAIL_TEST = jmap/jmap_email_test
JMAP_BLOB_TEST = jmap/jmap_blob_test
JMAP_SESSION_TEST = jmap/jmap_session_test
SMIME_TEST = smime/smime_low_level_test
MAILPGP_TEST = mailpgp/mailpgp_low_level_test
TEST_PROGRAMS = \
	$(IMAP_TEST) \
	$(IMF_TEST) \
	$(MIME_TEST) \
	$(MIME_BUILDER_TEST) \
	$(MIME_PARSER_SERIALIZATION_TEST) \
	$(CHARSET_DETECTION_TEST) \
	$(PLAINTEXT_RENDERING_TEST) \
	$(IMAP_UTF7_TEST) \
	$(DATA_TYPES_TEST) \
	$(CHARCONV_TEST) \
	$(MAILHTTP_TEST) \
	$(FEED_HTTP_TEST) \
	$(ACTIVESYNC_WBXML_TEST) \
	$(ACTIVESYNC_HTTP_TEST) \
	$(ACTIVESYNC_SAMPLE_TEST) \
	$(POP3_TEST) \
	$(GMAIL_TEST) \
	$(JMAP_JSON_TEST) \
	$(JMAP_HTTP_TEST) \
	$(JMAP_REQUEST_TEST) \
	$(JMAP_RESPONSE_TEST) \
	$(JMAP_CALL_TEST) \
	$(JMAP_MAILBOX_TEST) \
	$(JMAP_THREAD_TEST) \
	$(JMAP_EMAIL_TEST) \
	$(JMAP_BLOB_TEST) \
	$(JMAP_SESSION_TEST) \
	$(SMIME_TEST) \
	$(MAILPGP_TEST)
COMMON_TEST_SRCS = \
	common/test_utils.c
IMAP_TEST_SRCS = \
	imap/main.c \
	imap/response_data_test.c \
	imap/response_done_test.c \
	imap/unsupported_response_test.c \
	imap/command_sender_test.c \
	imap/command_parameter_sender_test.c \
	imap/idle_test.c \
	imap/imap_test_utils.c
IMAP_TEST_DEPS = $(IMAP_TEST_SRCS) imap/*.h
IMF_TEST_SRCS = \
	imf/main.c \
	imf/parser_test.c
IMF_TEST_FIXTURES = \
	imf/data/fields/all-standard.imf \
	imf/data/messages/folded-comments-rfc822.eml \
	imf/data/messages/resent-trace-rfc822.eml \
	imf/data/messages/simple-rfc822.eml
IMF_TEST_DEPS = $(IMF_TEST_SRCS) imf/*.h common/test_case.h \
	$(IMF_TEST_FIXTURES)
MIME_TEST_SRCS = \
	mime/main.c \
	mime/parser_test.c
MIME_TEST_FIXTURES = \
	mime/data/fields/rfc2045-headers.eml \
	mime/data/messages/rfc822-alternative.eml \
	mime/data/messages/rfc822-multipart.eml
MIME_TEST_DEPS = $(MIME_TEST_SRCS) mime/*.h common/test_case.h \
	$(MIME_TEST_FIXTURES)
MIME_BUILDER_TEST_SRCS = \
	mime-builder/main.c \
	mime-builder/mime_builder_helpers.c \
	$(COMMON_TEST_SRCS)
MIME_PARSER_SERIALIZATION_TEST_SRCS = \
	mime-parser/main.c \
	mime-parser/mime_serializer.c \
	mime-parser/json_writer.c \
	$(COMMON_TEST_SRCS)
CHARSET_DETECTION_TEST_SRCS = \
	charset-detection/main.c \
	charset-detection/charset_detection_tests.c \
	$(COMMON_TEST_SRCS)
PLAINTEXT_RENDERING_TEST_SRCS = \
	plaintext-rendering/main.c \
	plaintext-rendering/html_cleaner.c \
	plaintext-rendering/html_flattener.c \
	plaintext-rendering/html_renderer.c \
	$(COMMON_TEST_SRCS)
IMAP_UTF7_TEST_SRCS = \
	imap-utf7/main.c \
	imap-utf7/imap_utf7_tests.c
DATA_TYPES_TEST_SRCS = \
	data-types/main.c \
	data-types/md5_test.c \
	$(TOP)/src/data-types/base64.c \
	$(TOP)/src/data-types/md5.c
CHARCONV_TEST_SRCS = charconv/main.c
MAILHTTP_TEST_SRCS = http/mailhttp_test.c
MAILHTTP_TEST_DEPS = $(MAILHTTP_TEST_SRCS) $(TOP)/src/data-types/mailhttp.h
FEED_HTTP_TEST_SRCS = feed/feed_http_test.c
FEED_HTTP_TEST_DEPS = $(FEED_HTTP_TEST_SRCS) \
	$(TOP)/src/low-level/feed/newsfeed.h
ACTIVESYNC_WBXML_TEST_SRCS = \
	activesync/wbxml_test.c
ACTIVESYNC_HTTP_TEST_SRCS = \
	activesync/http_test.c
ACTIVESYNC_SAMPLE_TEST_SRCS = \
	activesync/sample_test.c
POP3_TEST_SRCS = \
	pop3/main.c
GMAIL_TEST_SRCS = \
	gmail/http_test.c
JMAP_JSON_TEST_SRCS = \
	jmap/jmap-json-test.c
JMAP_HTTP_TEST_SRCS = \
	jmap/jmap-http-test.c
JMAP_REQUEST_TEST_SRCS = \
	jmap/jmap-request-test.c
JMAP_RESPONSE_TEST_SRCS = \
	jmap/jmap-response-test.c
JMAP_CALL_TEST_SRCS = \
	jmap/jmap-call-test.c
JMAP_MAILBOX_TEST_SRCS = \
	jmap/jmap-mailbox-test.c
JMAP_THREAD_TEST_SRCS = \
	jmap/jmap-thread-test.c
JMAP_EMAIL_TEST_SRCS = \
	jmap/jmap-email-test.c
JMAP_BLOB_TEST_SRCS = \
	jmap/jmap-blob-test.c
JMAP_SESSION_TEST_SRCS = \
	jmap/jmap-session-test.c
SMIME_TEST_SRCS = \
	smime/smime_low_level_test.c
MAILPGP_TEST_SRCS = \
	mailpgp/mailpgp_low_level_test.c
MIME_BUILDER_TEST_DEPS = $(MIME_BUILDER_TEST_SRCS) common/*.h \
	mime-builder/*.h \
	mime-builder/data/input/photo.jpg \
	mime-builder/data/input/photo2.jpg \
	mime-builder/data/output/builder1.eml \
	mime-builder/data/output/builder2.eml \
	mime-builder/data/output/builder3.eml
MIME_PARSER_SERIALIZATION_TEST_DEPS = \
	$(MIME_PARSER_SERIALIZATION_TEST_SRCS) mime-parser/*.h common/*.h
CHARSET_DETECTION_TEST_DEPS = $(CHARSET_DETECTION_TEST_SRCS) \
	charset-detection/*.h common/*.h charset-detection/data/input/*
PLAINTEXT_RENDERING_TEST_DEPS = $(PLAINTEXT_RENDERING_TEST_SRCS) \
	plaintext-rendering/*.h common/*.h plaintext-rendering/data/input/* \
	plaintext-rendering/data/output/*
IMAP_UTF7_TEST_DEPS = $(IMAP_UTF7_TEST_SRCS) imap-utf7/*.h common/test_case.h
DATA_TYPES_TEST_DEPS = $(DATA_TYPES_TEST_SRCS) \
	data-types/data_types_tests.h common/test_case.h \
        $(TOP)/src/data-types/base64.h \
        $(TOP)/src/data-types/md5.h \
        $(TOP)/src/data-types/md5global.h \
        $(TOP)/src/data-types/hmac-md5.h
CHARCONV_TEST_DEPS = $(CHARCONV_TEST_SRCS) \
	$(TOP)/src/data-types/charconv.h
ACTIVESYNC_WBXML_TEST_DEPS = $(ACTIVESYNC_WBXML_TEST_SRCS) \
	activesync/*.h common/test_case.h
ACTIVESYNC_HTTP_TEST_DEPS = $(ACTIVESYNC_HTTP_TEST_SRCS) \
	activesync/*.h common/test_case.h \
	$(TOP)/src/low-level/activesync/mailactivesync_command.h
ACTIVESYNC_SAMPLE_TEST_DEPS = $(ACTIVESYNC_SAMPLE_TEST_SRCS) \
	activesync/*.h common/test_case.h \
	$(TOP)/tests/activesync-sample.c \
	$(TOP)/src/low-level/activesync/mailactivesync_command.h
POP3_TEST_DEPS = $(POP3_TEST_SRCS) pop3/*.h common/test_case.h
GMAIL_TEST_DEPS = $(GMAIL_TEST_SRCS) common/test_case.h
JMAP_FIXTURES = $(wildcard jmap/data/*/*.json) \
	$(wildcard jmap/data/*/*.eml)
JMAP_JSON_TEST_DEPS = $(JMAP_JSON_TEST_SRCS) $(JMAP_FIXTURES) \
	common/test_case.h
JMAP_HTTP_TEST_DEPS = $(JMAP_HTTP_TEST_SRCS) common/test_case.h
JMAP_REQUEST_TEST_DEPS = $(JMAP_REQUEST_TEST_SRCS) $(JMAP_FIXTURES) \
	common/test_case.h
JMAP_RESPONSE_TEST_DEPS = $(JMAP_RESPONSE_TEST_SRCS) $(JMAP_FIXTURES) \
	common/test_case.h
JMAP_CALL_TEST_DEPS = $(JMAP_CALL_TEST_SRCS) common/test_case.h
JMAP_MAILBOX_TEST_DEPS = $(JMAP_MAILBOX_TEST_SRCS) $(JMAP_FIXTURES) \
	common/test_case.h
JMAP_THREAD_TEST_DEPS = $(JMAP_THREAD_TEST_SRCS) common/test_case.h
JMAP_EMAIL_TEST_DEPS = $(JMAP_EMAIL_TEST_SRCS) $(JMAP_FIXTURES) \
	common/test_case.h
JMAP_BLOB_TEST_DEPS = $(JMAP_BLOB_TEST_SRCS) $(JMAP_FIXTURES) \
	common/test_case.h
JMAP_SESSION_TEST_DEPS = $(JMAP_SESSION_TEST_SRCS) $(JMAP_FIXTURES) \
	common/test_case.h
SMIME_TEST_DEPS = $(SMIME_TEST_SRCS) smime/fixtures/* common/test_case.h
MAILPGP_TEST_DEPS = $(MAILPGP_TEST_SRCS) mailpgp/fixtures/* common/test_case.h


.PHONY: all check check-public-headers check-xctest clean

all: $(TEST_PROGRAMS)

libetpan:
	cd .. && make

check: all
	@cd imap && ./imap_test
	@cd imf && ./imf_test
	@cd mime && ./mime_test
	@cd mime-builder && ./mime_builder_test
	@cd mime-parser && ./mime_parser_serialization_test
	@cd charset-detection && ./charset_detection_test
	@cd plaintext-rendering && ./plaintext_rendering_test
	@cd imap-utf7 && ./imap_utf7_test
	@cd data-types && ./data_types_test
	@cd charconv && ./charconv_test
	@cd http && ./http_test
	@cd feed && ./feed_http_test
	@cd activesync && ./wbxml_test
	@cd activesync && ./http_test
	@cd activesync && ./sample_test
	@cd pop3 && ./pop3_test
	@cd gmail && ./gmail_test
	@./jmap/jmap_json_test
	@./jmap/jmap_http_test
	@./jmap/jmap_request_test
	@./jmap/jmap_response_test
	@./jmap/jmap_call_test
	@./jmap/jmap_mailbox_test
	@./jmap/jmap_thread_test
	@./jmap/jmap_email_test
	@./jmap/jmap_blob_test
	@./jmap/jmap_session_test
	@cd .. && ./unittest/smime/smime_low_level_test
	@log=$${TMPDIR:-/tmp}/libetpan-mailpgp-low-level-test.log; \
	cd .. && ./unittest/mailpgp/mailpgp_low_level_test 2>$$log; \
	status=$$?; \
	if test $$status -ne 0; then cat $$log; exit $$status; fi; \
	rm -f $$log; \
	echo "mailpgp-low-level-test: ok"
	@$(MAKE) check-public-headers
	@$(MAKE) check-xctest

check-public-headers:
	cd .. && $(MAKE) public-headers-check

check-xctest:
	@if command -v $(XCODEBUILD) >/dev/null 2>&1; then \
		cd .. && $(XCODEBUILD) test \
			-project "build-mac/libetpan Tests.xcodeproj" \
			-scheme libetpan-unit-tests; \
	else \
		echo "xctest: skipped (xcodebuild not found)"; \
	fi

$(IMAP_TEST): $(IMAP_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAP_TEST_SRCS) $(LDLIBS) -o $@

$(IMF_TEST): $(IMF_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMF_TEST_SRCS) $(LDLIBS) -o $@

$(MIME_TEST): $(MIME_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MIME_TEST_SRCS) $(LDLIBS) -o $@

$(MIME_BUILDER_TEST): $(MIME_BUILDER_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MIME_BUILDER_TEST_SRCS) $(LDLIBS) -o $@

$(MIME_PARSER_SERIALIZATION_TEST): $(MIME_PARSER_SERIALIZATION_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MIME_PARSER_SERIALIZATION_TEST_SRCS) $(LDLIBS) -o $@

$(CHARSET_DETECTION_TEST): $(CHARSET_DETECTION_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CHARSET_DETECTION_TEST_SRCS) $(LDLIBS) -o $@

$(PLAINTEXT_RENDERING_TEST): $(PLAINTEXT_RENDERING_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(XML2_CPPFLAGS) $(TIDY_CPPFLAGS) $(CFLAGS) $(PLAINTEXT_RENDERING_TEST_SRCS) $(LDLIBS) $(XML2_LDLIBS) $(TIDY_LDLIBS) $(MATH_LDLIBS) -o $@

$(IMAP_UTF7_TEST): $(IMAP_UTF7_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAP_UTF7_TEST_SRCS) $(LDLIBS) -o $@

$(DATA_TYPES_TEST): $(DATA_TYPES_TEST_DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DATA_TYPES_TEST_SRCS) -o $@

$(CHARCONV_TEST): $(CHARCONV_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CHARCONV_TEST_SRCS) $(LDLIBS) -o $@

$(MAILHTTP_TEST): $(MAILHTTP_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MAILHTTP_TEST_SRCS) $(LDLIBS) -o $@

$(FEED_HTTP_TEST): $(FEED_HTTP_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(XML2_CPPFLAGS) $(CFLAGS) $(FEED_HTTP_TEST_SRCS) \
		$(LDLIBS) $(XML2_LDLIBS) -o $@

$(ACTIVESYNC_WBXML_TEST): $(ACTIVESYNC_WBXML_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ACTIVESYNC_WBXML_TEST_SRCS) $(LDLIBS) -o $@

$(ACTIVESYNC_HTTP_TEST): $(ACTIVESYNC_HTTP_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ACTIVESYNC_HTTP_TEST_SRCS) $(LDLIBS) -o $@

$(ACTIVESYNC_SAMPLE_TEST): $(ACTIVESYNC_SAMPLE_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ACTIVESYNC_SAMPLE_TEST_SRCS) $(LDLIBS) -o $@

$(POP3_TEST): $(POP3_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(POP3_TEST_SRCS) $(LDLIBS) -o $@

$(GMAIL_TEST): $(GMAIL_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GMAIL_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(JMAP_JSON_TEST): $(JMAP_JSON_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JMAP_JSON_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(JMAP_HTTP_TEST): $(JMAP_HTTP_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JMAP_HTTP_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(JMAP_REQUEST_TEST): $(JMAP_REQUEST_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JMAP_REQUEST_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(JMAP_RESPONSE_TEST): $(JMAP_RESPONSE_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JMAP_RESPONSE_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(JMAP_CALL_TEST): $(JMAP_CALL_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JMAP_CALL_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(JMAP_MAILBOX_TEST): $(JMAP_MAILBOX_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JMAP_MAILBOX_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(JMAP_THREAD_TEST): $(JMAP_THREAD_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JMAP_THREAD_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(JMAP_EMAIL_TEST): $(JMAP_EMAIL_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JMAP_EMAIL_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(JMAP_BLOB_TEST): $(JMAP_BLOB_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JMAP_BLOB_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(JMAP_SESSION_TEST): $(JMAP_SESSION_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JMAP_SESSION_TEST_SRCS) $(LDLIBS) $(JSON_C_LDLIBS) -o $@

$(SMIME_TEST): $(SMIME_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SMIME_TEST_SRCS) $(LDLIBS) $(OPENSSL_LDLIBS) -o $@

$(MAILPGP_TEST): $(MAILPGP_TEST_DEPS) $(LIBETPAN_TEST_DEPENDENCY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MAILPGP_TEST_SRCS) $(LDLIBS) $(RNP_LDLIBS) -o $@

clean:
	rm -f $(IMAP_TEST) $(IMF_TEST) $(MIME_TEST) $(MIME_BUILDER_TEST)
	rm -f $(MIME_PARSER_SERIALIZATION_TEST) $(CHARSET_DETECTION_TEST)
	rm -f $(PLAINTEXT_RENDERING_TEST) $(IMAP_UTF7_TEST)
	rm -f $(DATA_TYPES_TEST) $(CHARCONV_TEST)
	rm -f $(MAILHTTP_TEST)
	rm -f $(FEED_HTTP_TEST)
	rm -f $(ACTIVESYNC_WBXML_TEST) $(ACTIVESYNC_HTTP_TEST)
	rm -f $(ACTIVESYNC_SAMPLE_TEST) $(POP3_TEST)
	rm -f $(GMAIL_TEST) $(JMAP_JSON_TEST) $(JMAP_HTTP_TEST)
	rm -f $(JMAP_REQUEST_TEST) $(JMAP_RESPONSE_TEST)
	rm -f $(JMAP_CALL_TEST) $(JMAP_MAILBOX_TEST)
	rm -f $(JMAP_THREAD_TEST) $(JMAP_EMAIL_TEST)
	rm -f $(JMAP_BLOB_TEST) $(JMAP_SESSION_TEST)
	rm -f $(SMIME_TEST) $(MAILPGP_TEST)
	rm -rf $(IMAP_TEST).dSYM
	rm -rf $(IMF_TEST).dSYM
	rm -rf $(MIME_TEST).dSYM
	rm -rf $(MIME_BUILDER_TEST).dSYM
	rm -rf $(MIME_PARSER_SERIALIZATION_TEST).dSYM
	rm -rf $(CHARSET_DETECTION_TEST).dSYM
	rm -rf $(PLAINTEXT_RENDERING_TEST).dSYM
	rm -rf $(IMAP_UTF7_TEST).dSYM
	rm -rf $(DATA_TYPES_TEST).dSYM
	rm -rf $(CHARCONV_TEST).dSYM
	rm -rf $(ACTIVESYNC_WBXML_TEST).dSYM
	rm -rf $(ACTIVESYNC_HTTP_TEST).dSYM
	rm -rf $(ACTIVESYNC_SAMPLE_TEST).dSYM
	rm -rf $(POP3_TEST).dSYM
	rm -rf $(GMAIL_TEST).dSYM
	rm -rf $(JMAP_JSON_TEST).dSYM
	rm -rf $(JMAP_HTTP_TEST).dSYM
	rm -rf $(JMAP_REQUEST_TEST).dSYM
	rm -rf $(JMAP_RESPONSE_TEST).dSYM
	rm -rf $(JMAP_CALL_TEST).dSYM
	rm -rf $(JMAP_MAILBOX_TEST).dSYM
	rm -rf $(JMAP_THREAD_TEST).dSYM
	rm -rf $(JMAP_EMAIL_TEST).dSYM
	rm -rf $(JMAP_BLOB_TEST).dSYM
	rm -rf $(JMAP_SESSION_TEST).dSYM
	rm -rf $(SMIME_TEST).dSYM
	rm -rf $(MAILPGP_TEST).dSYM

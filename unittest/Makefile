CC ?= cc

TOP = ..

CPPFLAGS += \
	-I$(TOP) \
	-I$(TOP)/include \
	-I$(TOP)/src/data-types \
	-I$(TOP)/src/low-level/imf \
	-I$(TOP)/src/low-level/imap \
	-I$(TOP)/src/low-level/mime \
	-Icommon

CFLAGS += -Wall -Wextra -Werror -g
LDLIBS += $(TOP)/src/.libs/libetpan.a -lz -lsasl2 -lpthread
XML2_CPPFLAGS := -I$(shell xcrun --show-sdk-path)/usr/include/libxml2
XML2_LDLIBS := -lxml2
TIDY_CPPFLAGS := $(shell pkg-config --cflags tidy 2>/dev/null || printf '%s' "-I$$(brew --prefix tidy-html5)/include") -I /usr/include/tidy
TIDY_LDLIBS := $(shell pkg-config --libs tidy 2>/dev/null || printf '%s' "-L$$(brew --prefix tidy-html5)/lib -ltidy")
MATH_LDLIBS := -lm

IMAP_TEST = imap/imap_test
IMF_TEST = imf/imf_test
MIME_TEST = mime/mime_test
MIME_BUILDER_TEST = mime-builder/mime_builder_test
MIME_PARSER_SERIALIZATION_TEST = mime-parser/mime_parser_serialization_test
CHARSET_DETECTION_TEST = charset-detection/charset_detection_test
PLAINTEXT_RENDERING_TEST = plaintext-rendering/plaintext_rendering_test
IMAP_UTF7_TEST = imap-utf7/imap_utf7_test
DATA_TYPES_TEST = data-types/data_types_test
COMMON_TEST_SRCS = \
	common/test_utils.c
IMAP_TEST_SRCS = \
	imap/main.c \
	imap/response_data_test.c \
	imap/response_done_test.c \
	imap/unsupported_response_test.c \
	imap/command_sender_test.c \
	imap/command_parameter_sender_test.c \
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
IMF_TEST_DEPS = $(IMF_TEST_SRCS) imf/*.h $(IMF_TEST_FIXTURES)
MIME_TEST_SRCS = \
	mime/main.c \
	mime/parser_test.c
MIME_TEST_FIXTURES = \
	mime/data/fields/rfc2045-headers.eml \
	mime/data/messages/rfc822-alternative.eml \
	mime/data/messages/rfc822-multipart.eml
MIME_TEST_DEPS = $(MIME_TEST_SRCS) mime/*.h $(MIME_TEST_FIXTURES)
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
	$(COMMON_TEST_SRCS)
PLAINTEXT_RENDERING_TEST_SRCS = \
	plaintext-rendering/main.c \
	plaintext-rendering/html_cleaner.c \
	plaintext-rendering/html_flattener.c \
	plaintext-rendering/html_renderer.c \
	$(COMMON_TEST_SRCS)
IMAP_UTF7_TEST_SRCS = \
	imap-utf7/main.c
DATA_TYPES_TEST_SRCS = \
	data-types/md5_test.c \
	$(TOP)/src/data-types/md5.c
MIME_BUILDER_TEST_DEPS = $(MIME_BUILDER_TEST_SRCS) common/*.h \
	mime-builder/data/input/photo.jpg \
	mime-builder/data/input/photo2.jpg \
	mime-builder/data/output/builder1.eml \
	mime-builder/data/output/builder2.eml \
	mime-builder/data/output/builder3.eml
MIME_PARSER_SERIALIZATION_TEST_DEPS = \
	$(MIME_PARSER_SERIALIZATION_TEST_SRCS) common/*.h
CHARSET_DETECTION_TEST_DEPS = $(CHARSET_DETECTION_TEST_SRCS) common/*.h
PLAINTEXT_RENDERING_TEST_DEPS = $(PLAINTEXT_RENDERING_TEST_SRCS) common/*.h
IMAP_UTF7_TEST_DEPS = $(IMAP_UTF7_TEST_SRCS)
DATA_TYPES_TEST_DEPS = $(DATA_TYPES_TEST_SRCS) \
        $(TOP)/src/data-types/md5.h \
        $(TOP)/src/data-types/md5global.h \
        $(TOP)/src/data-types/hmac-md5.h


.PHONY: all check clean

all: $(IMAP_TEST) $(IMF_TEST) $(MIME_TEST) $(MIME_BUILDER_TEST) \
	$(MIME_PARSER_SERIALIZATION_TEST) $(CHARSET_DETECTION_TEST) \
	$(PLAINTEXT_RENDERING_TEST) $(IMAP_UTF7_TEST) $(DATA_TYPES_TEST)

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

$(IMAP_TEST): $(IMAP_TEST_DEPS) libetpan
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAP_TEST_SRCS) $(LDLIBS) -o $@

$(IMF_TEST): $(IMF_TEST_DEPS) libetpan
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMF_TEST_SRCS) $(LDLIBS) -o $@

$(MIME_TEST): $(MIME_TEST_DEPS) libetpan
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MIME_TEST_SRCS) $(LDLIBS) -o $@

$(MIME_BUILDER_TEST): $(MIME_BUILDER_TEST_DEPS) libetpan
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MIME_BUILDER_TEST_SRCS) $(LDLIBS) -o $@

$(MIME_PARSER_SERIALIZATION_TEST): $(MIME_PARSER_SERIALIZATION_TEST_DEPS) libetpan
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MIME_PARSER_SERIALIZATION_TEST_SRCS) $(LDLIBS) -o $@

$(CHARSET_DETECTION_TEST): $(CHARSET_DETECTION_TEST_DEPS) libetpan
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CHARSET_DETECTION_TEST_SRCS) $(LDLIBS) -o $@

$(PLAINTEXT_RENDERING_TEST): $(PLAINTEXT_RENDERING_TEST_DEPS) libetpan
	$(CC) $(CPPFLAGS) $(XML2_CPPFLAGS) $(TIDY_CPPFLAGS) $(CFLAGS) $(PLAINTEXT_RENDERING_TEST_SRCS) $(LDLIBS) $(XML2_LDLIBS) $(TIDY_LDLIBS) $(MATH_LDLIBS) -o $@

$(IMAP_UTF7_TEST): $(IMAP_UTF7_TEST_DEPS) libetpan
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAP_UTF7_TEST_SRCS) $(LDLIBS) -o $@

$(DATA_TYPES_TEST): $(DATA_TYPES_TEST_DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DATA_TYPES_TEST_SRCS) -o $@

clean:
	rm -f $(IMAP_TEST) $(IMF_TEST) $(MIME_TEST) $(MIME_BUILDER_TEST)
	rm -f $(MIME_PARSER_SERIALIZATION_TEST) $(CHARSET_DETECTION_TEST)
	rm -f $(PLAINTEXT_RENDERING_TEST) $(IMAP_UTF7_TEST)
	rm -f $(DATA_TYPES_TEST)
	rm -rf $(IMAP_TEST).dSYM
	rm -rf $(IMF_TEST).dSYM
	rm -rf $(MIME_TEST).dSYM
	rm -rf $(MIME_BUILDER_TEST).dSYM
	rm -rf $(MIME_PARSER_SERIALIZATION_TEST).dSYM
	rm -rf $(CHARSET_DETECTION_TEST).dSYM
	rm -rf $(PLAINTEXT_RENDERING_TEST).dSYM
	rm -rf $(IMAP_UTF7_TEST).dSYM
	rm -rf $(DATA_TYPES_TEST).dSYM

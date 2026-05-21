CC ?= cc

TOP = ..

CPPFLAGS += \
	-I$(TOP) \
	-I$(TOP)/include \
	-I$(TOP)/src/data-types \
	-I$(TOP)/src/low-level/imf \
	-I$(TOP)/src/low-level/imap

CFLAGS += -Wall -Wextra -Werror -g
LDLIBS += $(TOP)/src/.libs/libetpan.a -lz -liconv -lsasl2 -lpthread

IMAP_TEST = imap/imap_test
IMF_TEST = imf/imf_test
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

.PHONY: all check clean

all: $(IMAP_TEST) $(IMF_TEST)

check: all
	@cd imap && ./imap_test
	@cd imf && ./imf_test

$(IMAP_TEST): $(IMAP_TEST_DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAP_TEST_SRCS) $(LDLIBS) -o $@

$(IMF_TEST): $(IMF_TEST_DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMF_TEST_SRCS) $(LDLIBS) -o $@

clean:
	rm -f $(IMAP_TEST) $(IMF_TEST)
	rm -rf $(IMAP_TEST).dSYM
	rm -rf $(IMF_TEST).dSYM

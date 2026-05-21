CC ?= cc

TOP = ..

CPPFLAGS += \
	-I$(TOP) \
	-I$(TOP)/include \
	-I$(TOP)/src/data-types \
	-I$(TOP)/src/low-level/imap

CFLAGS += -Wall -Wextra -Werror -g
LDLIBS += $(TOP)/src/.libs/libetpan.a -lz -liconv -lsasl2 -lpthread

IMAP_TEST = imap/imap_test
IMAP_TEST_SRCS = \
	imap/main.c \
	imap/response_data_test.c \
	imap/response_done_test.c \
	imap/unsupported_response_test.c \
	imap/command_sender_test.c \
	imap/command_parameter_sender_test.c \
	imap/imap_test_utils.c
IMAP_TEST_DEPS = $(IMAP_TEST_SRCS) imap/*.h

.PHONY: all check clean

all: $(IMAP_TEST)

check: all
	@cd imap && ./imap_test

$(IMAP_TEST): $(IMAP_TEST_DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAP_TEST_SRCS) $(LDLIBS) -o $@

clean:
	rm -f $(IMAP_TEST)
	rm -rf $(IMAP_TEST).dSYM

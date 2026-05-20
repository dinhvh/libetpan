CC ?= cc

TOP = ..

CPPFLAGS += \
	-I$(TOP) \
	-I$(TOP)/include \
	-I$(TOP)/src/data-types \
	-I$(TOP)/src/low-level/imap

CFLAGS += -Wall -Wextra -Werror -g
LDLIBS += $(TOP)/src/.libs/libetpan.a -lz -liconv -lsasl2 -lpthread

TESTS = response_data_test response_done_test unsupported_response_test \
	command_sender_test command_parameter_sender_test
UTILS = imap_test_utils.c imap_test_utils.h

.PHONY: all check clean

all: $(TESTS)

check: all
	@for test in $(TESTS); do ./$$test || exit $$?; done

response_data_test: response_data_test.c $(UTILS)
	$(CC) $(CPPFLAGS) $(CFLAGS) response_data_test.c imap_test_utils.c $(LDLIBS) -o $@

response_done_test: response_done_test.c $(UTILS)
	$(CC) $(CPPFLAGS) $(CFLAGS) response_done_test.c imap_test_utils.c $(LDLIBS) -o $@

unsupported_response_test: unsupported_response_test.c $(UTILS)
	$(CC) $(CPPFLAGS) $(CFLAGS) unsupported_response_test.c imap_test_utils.c $(LDLIBS) -o $@

command_sender_test: command_sender_test.c $(UTILS)
	$(CC) $(CPPFLAGS) $(CFLAGS) command_sender_test.c imap_test_utils.c $(LDLIBS) -o $@

command_parameter_sender_test: command_parameter_sender_test.c $(UTILS)
	$(CC) $(CPPFLAGS) $(CFLAGS) command_parameter_sender_test.c imap_test_utils.c $(LDLIBS) -o $@

clean:
	rm -f $(TESTS) imap_response_parser_test
	rm -rf $(TESTS:=.dSYM) imap_response_parser_test.dSYM

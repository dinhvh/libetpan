#ifndef MAILSMTP_PRIVATE_H

#define MAILSMTP_PRIVATE_H

#include <libetpan/mailsmtp_types.h>

int mailsmtp_send_command(mailsmtp * f, char * command);

int mailsmtp_read_response(mailsmtp * session);

#endif

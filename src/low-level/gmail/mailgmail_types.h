/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILGMAIL_TYPES_H

#define MAILGMAIL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/clist.h>

#include <stddef.h>
#include <stdint.h>
#include <time.h>

enum {
  MAILGMAIL_NO_ERROR = 0,
  MAILGMAIL_ERROR_BAD_STATE,
  MAILGMAIL_ERROR_UNAUTHORIZED,
  MAILGMAIL_ERROR_HTTP,
  MAILGMAIL_ERROR_PROTOCOL,
  MAILGMAIL_ERROR_PARSE,
  MAILGMAIL_ERROR_MEMORY,
  MAILGMAIL_ERROR_SSL,
  MAILGMAIL_ERROR_NOT_IMPLEMENTED,
  MAILGMAIL_ERROR_HTTP_UNAVAILABLE,
  MAILGMAIL_ERROR_RATE_LIMITED,
  MAILGMAIL_ERROR_NOT_FOUND,
  MAILGMAIL_ERROR_FORBIDDEN,
  MAILGMAIL_ERROR_CONFLICT,
  MAILGMAIL_ERROR_SERVER
};

enum mailgmail_message_format {
  MAILGMAIL_MESSAGE_FORMAT_FULL = 0,
  MAILGMAIL_MESSAGE_FORMAT_METADATA,
  MAILGMAIL_MESSAGE_FORMAT_MINIMAL,
  MAILGMAIL_MESSAGE_FORMAT_RAW
};

typedef struct mailgmail mailgmail;

struct mailgmail_profile {
  char * email_address;
  uint32_t messages_total;
  uint32_t threads_total;
  char * history_id;
};

struct mailgmail_label {
  char * id;
  char * name;
  char * type;
  char * message_list_visibility;
  char * label_list_visibility;
  uint32_t messages_total;
  uint32_t messages_unread;
  uint32_t threads_total;
  uint32_t threads_unread;
};

struct mailgmail_label_list {
  clist * labels; /* struct mailgmail_label * */
};

struct mailgmail_message_list_request {
  uint32_t max_results;
  char * page_token;
  char * query;
  clist * label_ids; /* char * */
  int include_spam_trash;
};

struct mailgmail_message_summary {
  char * id;
  char * thread_id;
};

struct mailgmail_message_list {
  clist * messages; /* struct mailgmail_message_summary * */
  char * next_page_token;
  uint32_t result_size_estimate;
};

struct mailgmail_message_get_request {
  enum mailgmail_message_format format;
  clist * metadata_headers; /* char * */
};

struct mailgmail_message_header {
  char * name;
  char * value;
};

struct mailgmail_message_part_body {
  char * attachment_id;
  uint32_t size;
  char * data;
};

struct mailgmail_message_part {
  char * part_id;
  char * mime_type;
  char * filename;
  clist * headers; /* struct mailgmail_message_header * */
  struct mailgmail_message_part_body * body;
  clist * parts; /* struct mailgmail_message_part * */
};

struct mailgmail_message {
  char * id;
  char * thread_id;
  clist * label_ids; /* char * */
  char * snippet;
  char * history_id;
  char * internal_date;
  uint32_t size_estimate;
  char * raw;
  struct mailgmail_message_part * payload;
};

struct mailgmail_attachment {
  char * attachment_id;
  uint32_t size;
  char * data;
};

LIBETPAN_EXPORT
struct mailgmail_profile * mailgmail_profile_new(void);

LIBETPAN_EXPORT
void mailgmail_profile_free(struct mailgmail_profile * profile);

LIBETPAN_EXPORT
struct mailgmail_label * mailgmail_label_new(void);

LIBETPAN_EXPORT
void mailgmail_label_free(struct mailgmail_label * label);

LIBETPAN_EXPORT
struct mailgmail_label_list * mailgmail_label_list_new(void);

LIBETPAN_EXPORT
void mailgmail_label_list_free(struct mailgmail_label_list * label_list);

LIBETPAN_EXPORT
struct mailgmail_message_list_request *
mailgmail_message_list_request_new(void);

LIBETPAN_EXPORT
void mailgmail_message_list_request_free(
    struct mailgmail_message_list_request * request);

LIBETPAN_EXPORT
int mailgmail_message_list_request_set_page_token(
    struct mailgmail_message_list_request * request,
    const char * page_token);

LIBETPAN_EXPORT
int mailgmail_message_list_request_set_query(
    struct mailgmail_message_list_request * request,
    const char * query);

LIBETPAN_EXPORT
int mailgmail_message_list_request_add_label_id(
    struct mailgmail_message_list_request * request,
    const char * label_id);

LIBETPAN_EXPORT
struct mailgmail_message_summary *
mailgmail_message_summary_new(const char * id, const char * thread_id);

LIBETPAN_EXPORT
void mailgmail_message_summary_free(
    struct mailgmail_message_summary * summary);

LIBETPAN_EXPORT
struct mailgmail_message_list * mailgmail_message_list_new(void);

LIBETPAN_EXPORT
void mailgmail_message_list_free(struct mailgmail_message_list * list);

LIBETPAN_EXPORT
struct mailgmail_message_get_request *
mailgmail_message_get_request_new(enum mailgmail_message_format format);

LIBETPAN_EXPORT
void mailgmail_message_get_request_free(
    struct mailgmail_message_get_request * request);

LIBETPAN_EXPORT
int mailgmail_message_get_request_add_metadata_header(
    struct mailgmail_message_get_request * request,
    const char * header_name);

LIBETPAN_EXPORT
struct mailgmail_message_header *
mailgmail_message_header_new(const char * name, const char * value);

LIBETPAN_EXPORT
void mailgmail_message_header_free(
    struct mailgmail_message_header * header);

LIBETPAN_EXPORT
struct mailgmail_message_part_body * mailgmail_message_part_body_new(void);

LIBETPAN_EXPORT
void mailgmail_message_part_body_free(
    struct mailgmail_message_part_body * body);

LIBETPAN_EXPORT
struct mailgmail_message_part * mailgmail_message_part_new(void);

LIBETPAN_EXPORT
void mailgmail_message_part_free(struct mailgmail_message_part * part);

LIBETPAN_EXPORT
struct mailgmail_message * mailgmail_message_new(void);

LIBETPAN_EXPORT
void mailgmail_message_free(struct mailgmail_message * message);

LIBETPAN_EXPORT
struct mailgmail_attachment * mailgmail_attachment_new(void);

LIBETPAN_EXPORT
void mailgmail_attachment_free(struct mailgmail_attachment * attachment);

#ifdef __cplusplus
}
#endif

#endif

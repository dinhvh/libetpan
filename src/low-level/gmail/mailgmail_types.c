/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailgmail_types.h"

#include <stdlib.h>
#include <string.h>

static char * mailgmail_strdup(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

static void mailgmail_free_string(void * value, void * data)
{
  (void) data;
  free(value);
}

static int mailgmail_clist_add_string(clist * list, const char * value)
{
  char * copy;

  if ((list == NULL) || (value == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  copy = mailgmail_strdup(value);
  if (copy == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  if (clist_append(list, copy) < 0) {
    free(copy);
    return MAILGMAIL_ERROR_MEMORY;
  }

  return MAILGMAIL_NO_ERROR;
}

struct mailgmail_profile * mailgmail_profile_new(void)
{
  struct mailgmail_profile * profile;

  profile = malloc(sizeof(* profile));
  if (profile == NULL)
    return NULL;

  profile->email_address = NULL;
  profile->messages_total = 0;
  profile->threads_total = 0;
  profile->history_id = NULL;
  return profile;
}

void mailgmail_profile_free(struct mailgmail_profile * profile)
{
  if (profile == NULL)
    return;

  free(profile->email_address);
  free(profile->history_id);
  free(profile);
}

struct mailgmail_label * mailgmail_label_new(void)
{
  struct mailgmail_label * label;

  label = malloc(sizeof(* label));
  if (label == NULL)
    return NULL;

  label->id = NULL;
  label->name = NULL;
  label->type = NULL;
  label->message_list_visibility = NULL;
  label->label_list_visibility = NULL;
  label->messages_total = 0;
  label->messages_unread = 0;
  label->threads_total = 0;
  label->threads_unread = 0;
  return label;
}

void mailgmail_label_free(struct mailgmail_label * label)
{
  if (label == NULL)
    return;

  free(label->id);
  free(label->name);
  free(label->type);
  free(label->message_list_visibility);
  free(label->label_list_visibility);
  free(label);
}

static void mailgmail_label_free_item(void * value, void * data)
{
  (void) data;
  mailgmail_label_free(value);
}

struct mailgmail_label_list * mailgmail_label_list_new(void)
{
  struct mailgmail_label_list * label_list;

  label_list = malloc(sizeof(* label_list));
  if (label_list == NULL)
    return NULL;

  label_list->labels = clist_new();
  if (label_list->labels == NULL) {
    mailgmail_label_list_free(label_list);
    return NULL;
  }

  return label_list;
}

void mailgmail_label_list_free(struct mailgmail_label_list * label_list)
{
  if (label_list == NULL)
    return;

  if (label_list->labels != NULL) {
    clist_foreach(label_list->labels, mailgmail_label_free_item, NULL);
    clist_free(label_list->labels);
  }
  free(label_list);
}

struct mailgmail_message_list_request *
mailgmail_message_list_request_new(void)
{
  struct mailgmail_message_list_request * request;

  request = malloc(sizeof(* request));
  if (request == NULL)
    return NULL;

  request->max_results = 0;
  request->page_token = NULL;
  request->query = NULL;
  request->label_ids = clist_new();
  request->include_spam_trash = 0;

  if (request->label_ids == NULL) {
    mailgmail_message_list_request_free(request);
    return NULL;
  }

  return request;
}

void mailgmail_message_list_request_free(
    struct mailgmail_message_list_request * request)
{
  if (request == NULL)
    return;

  free(request->page_token);
  free(request->query);
  if (request->label_ids != NULL) {
    clist_foreach(request->label_ids, mailgmail_free_string, NULL);
    clist_free(request->label_ids);
  }
  free(request);
}

int mailgmail_message_list_request_add_label_id(
    struct mailgmail_message_list_request * request,
    const char * label_id)
{
  if (request == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  return mailgmail_clist_add_string(request->label_ids, label_id);
}

static int replace_string(char ** target, const char * value)
{
  char * copy;

  if (target == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  copy = NULL;
  if (value != NULL) {
    copy = mailgmail_strdup(value);
    if (copy == NULL)
      return MAILGMAIL_ERROR_MEMORY;
  }

  free(* target);
  * target = copy;
  return MAILGMAIL_NO_ERROR;
}

int mailgmail_message_list_request_set_page_token(
    struct mailgmail_message_list_request * request,
    const char * page_token)
{
  if (request == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  return replace_string(&request->page_token, page_token);
}

int mailgmail_message_list_request_set_query(
    struct mailgmail_message_list_request * request,
    const char * query)
{
  if (request == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  return replace_string(&request->query, query);
}

struct mailgmail_message_summary *
mailgmail_message_summary_new(const char * id, const char * thread_id)
{
  struct mailgmail_message_summary * summary;

  if ((id == NULL) || (thread_id == NULL))
    return NULL;

  summary = malloc(sizeof(* summary));
  if (summary == NULL)
    return NULL;

  summary->id = mailgmail_strdup(id);
  summary->thread_id = mailgmail_strdup(thread_id);
  if ((summary->id == NULL) || (summary->thread_id == NULL)) {
    mailgmail_message_summary_free(summary);
    return NULL;
  }

  return summary;
}

void mailgmail_message_summary_free(
    struct mailgmail_message_summary * summary)
{
  if (summary == NULL)
    return;

  free(summary->id);
  free(summary->thread_id);
  free(summary);
}

static void mailgmail_message_summary_free_item(void * value, void * data)
{
  (void) data;
  mailgmail_message_summary_free(value);
}

struct mailgmail_message_list * mailgmail_message_list_new(void)
{
  struct mailgmail_message_list * list;

  list = malloc(sizeof(* list));
  if (list == NULL)
    return NULL;

  list->messages = clist_new();
  list->next_page_token = NULL;
  list->result_size_estimate = 0;
  if (list->messages == NULL) {
    mailgmail_message_list_free(list);
    return NULL;
  }

  return list;
}

void mailgmail_message_list_free(struct mailgmail_message_list * list)
{
  if (list == NULL)
    return;

  if (list->messages != NULL) {
    clist_foreach(list->messages, mailgmail_message_summary_free_item, NULL);
    clist_free(list->messages);
  }
  free(list->next_page_token);
  free(list);
}

struct mailgmail_message_get_request *
mailgmail_message_get_request_new(enum mailgmail_message_format format)
{
  struct mailgmail_message_get_request * request;

  request = malloc(sizeof(* request));
  if (request == NULL)
    return NULL;

  request->format = format;
  request->metadata_headers = clist_new();
  if (request->metadata_headers == NULL) {
    mailgmail_message_get_request_free(request);
    return NULL;
  }

  return request;
}

void mailgmail_message_get_request_free(
    struct mailgmail_message_get_request * request)
{
  if (request == NULL)
    return;

  if (request->metadata_headers != NULL) {
    clist_foreach(request->metadata_headers, mailgmail_free_string, NULL);
    clist_free(request->metadata_headers);
  }
  free(request);
}

int mailgmail_message_get_request_add_metadata_header(
    struct mailgmail_message_get_request * request,
    const char * header_name)
{
  if (request == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  return mailgmail_clist_add_string(request->metadata_headers, header_name);
}

struct mailgmail_message_header *
mailgmail_message_header_new(const char * name, const char * value)
{
  struct mailgmail_message_header * header;

  if ((name == NULL) || (value == NULL))
    return NULL;

  header = malloc(sizeof(* header));
  if (header == NULL)
    return NULL;

  header->name = mailgmail_strdup(name);
  header->value = mailgmail_strdup(value);
  if ((header->name == NULL) || (header->value == NULL)) {
    mailgmail_message_header_free(header);
    return NULL;
  }

  return header;
}

void mailgmail_message_header_free(
    struct mailgmail_message_header * header)
{
  if (header == NULL)
    return;

  free(header->name);
  free(header->value);
  free(header);
}

static void mailgmail_message_header_free_item(void * value, void * data)
{
  (void) data;
  mailgmail_message_header_free(value);
}

struct mailgmail_message_part_body * mailgmail_message_part_body_new(void)
{
  struct mailgmail_message_part_body * body;

  body = malloc(sizeof(* body));
  if (body == NULL)
    return NULL;

  body->attachment_id = NULL;
  body->size = 0;
  body->data = NULL;
  return body;
}

void mailgmail_message_part_body_free(
    struct mailgmail_message_part_body * body)
{
  if (body == NULL)
    return;

  free(body->attachment_id);
  free(body->data);
  free(body);
}

struct mailgmail_message_part * mailgmail_message_part_new(void)
{
  struct mailgmail_message_part * part;

  part = malloc(sizeof(* part));
  if (part == NULL)
    return NULL;

  part->part_id = NULL;
  part->mime_type = NULL;
  part->filename = NULL;
  part->headers = clist_new();
  part->body = NULL;
  part->parts = clist_new();
  if ((part->headers == NULL) || (part->parts == NULL)) {
    mailgmail_message_part_free(part);
    return NULL;
  }

  return part;
}

static void mailgmail_message_part_free_item(void * value, void * data)
{
  (void) data;
  mailgmail_message_part_free(value);
}

void mailgmail_message_part_free(struct mailgmail_message_part * part)
{
  if (part == NULL)
    return;

  free(part->part_id);
  free(part->mime_type);
  free(part->filename);
  if (part->headers != NULL) {
    clist_foreach(part->headers, mailgmail_message_header_free_item, NULL);
    clist_free(part->headers);
  }
  mailgmail_message_part_body_free(part->body);
  if (part->parts != NULL) {
    clist_foreach(part->parts, mailgmail_message_part_free_item, NULL);
    clist_free(part->parts);
  }
  free(part);
}

struct mailgmail_message * mailgmail_message_new(void)
{
  struct mailgmail_message * message;

  message = malloc(sizeof(* message));
  if (message == NULL)
    return NULL;

  message->id = NULL;
  message->thread_id = NULL;
  message->label_ids = clist_new();
  message->snippet = NULL;
  message->history_id = NULL;
  message->internal_date = NULL;
  message->size_estimate = 0;
  message->raw = NULL;
  message->payload = NULL;
  if (message->label_ids == NULL) {
    mailgmail_message_free(message);
    return NULL;
  }

  return message;
}

void mailgmail_message_free(struct mailgmail_message * message)
{
  if (message == NULL)
    return;

  free(message->id);
  free(message->thread_id);
  if (message->label_ids != NULL) {
    clist_foreach(message->label_ids, mailgmail_free_string, NULL);
    clist_free(message->label_ids);
  }
  free(message->snippet);
  free(message->history_id);
  free(message->internal_date);
  free(message->raw);
  mailgmail_message_part_free(message->payload);
  free(message);
}

struct mailgmail_attachment * mailgmail_attachment_new(void)
{
  struct mailgmail_attachment * attachment;

  attachment = malloc(sizeof(* attachment));
  if (attachment == NULL)
    return NULL;

  attachment->attachment_id = NULL;
  attachment->size = 0;
  attachment->data = NULL;
  return attachment;
}

void mailgmail_attachment_free(struct mailgmail_attachment * attachment)
{
  if (attachment == NULL)
    return;

  free(attachment->attachment_id);
  free(attachment->data);
  free(attachment);
}

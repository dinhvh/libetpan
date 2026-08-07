/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjmap.h"
#include "mailjmap_json.h"
#include "mailjmap_request.h"
#include "mailjmap_response.h"

#include <stdlib.h>
#include <string.h>

#define MAILJMAP_CAPABILITY_MAIL "urn:ietf:params:jmap:mail"
#define MAILJMAP_CAPABILITY_SUBMISSION "urn:ietf:params:jmap:submission"

static void string_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    free(clist_content(cur));
  clist_free(list);
}

static int replace_string(char ** target, const char * value);

struct mailjmap_mailbox * mailjmap_mailbox_new(void)
{
  struct mailjmap_mailbox * mailbox;

  mailbox = malloc(sizeof(* mailbox));
  if (mailbox == NULL)
    return NULL;

  mailbox->id = NULL;
  mailbox->name = NULL;
  mailbox->parent_id = NULL;
  mailbox->role = NULL;
  mailbox->sort_order = 0;
  mailbox->is_subscribed = 0;
  mailbox->total_emails = 0;
  mailbox->unread_emails = 0;
  mailbox->total_threads = 0;
  mailbox->unread_threads = 0;
  mailbox->my_rights = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYALL);
  if (mailbox->my_rights == NULL) {
    mailjmap_mailbox_free(mailbox);
    return NULL;
  }
  return mailbox;
}

void mailjmap_mailbox_free(struct mailjmap_mailbox * mailbox)
{
  if (mailbox == NULL)
    return;

  free(mailbox->id);
  free(mailbox->name);
  free(mailbox->parent_id);
  free(mailbox->role);
  if (mailbox->my_rights != NULL)
    chash_free(mailbox->my_rights);
  free(mailbox);
}

struct mailjmap_thread * mailjmap_thread_new(void)
{
  struct mailjmap_thread * thread;

  thread = malloc(sizeof(* thread));
  if (thread == NULL)
    return NULL;

  thread->id = NULL;
  thread->email_ids = clist_new();
  if (thread->email_ids == NULL) {
    mailjmap_thread_free(thread);
    return NULL;
  }

  return thread;
}

void mailjmap_thread_free(struct mailjmap_thread * thread)
{
  if (thread == NULL)
    return;

  free(thread->id);
  string_list_free(thread->email_ids);
  free(thread);
}

struct mailjmap_email_header * mailjmap_email_header_new(void)
{
  struct mailjmap_email_header * header;

  header = malloc(sizeof(* header));
  if (header == NULL)
    return NULL;

  header->name = NULL;
  header->value = NULL;
  return header;
}

void mailjmap_email_header_free(struct mailjmap_email_header * header)
{
  if (header == NULL)
    return;

  free(header->name);
  free(header->value);
  free(header);
}

struct mailjmap_email_address * mailjmap_email_address_new(void)
{
  struct mailjmap_email_address * address;

  address = malloc(sizeof(* address));
  if (address == NULL)
    return NULL;

  address->name = NULL;
  address->email = NULL;
  return address;
}

void mailjmap_email_address_free(struct mailjmap_email_address * address)
{
  if (address == NULL)
    return;

  free(address->name);
  free(address->email);
  free(address);
}

static void email_address_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_address_free(clist_content(cur));
  clist_free(list);
}

static void email_header_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_header_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_email_body_part * mailjmap_email_body_part_new(void)
{
  struct mailjmap_email_body_part * part;

  part = malloc(sizeof(* part));
  if (part == NULL)
    return NULL;

  part->part_id = NULL;
  part->blob_id = NULL;
  part->type = NULL;
  part->name = NULL;
  part->charset = NULL;
  part->disposition = NULL;
  part->cid = NULL;
  part->language = NULL;
  part->languages = clist_new();
  part->location = NULL;
  part->size = 0;
  part->has_size = 0;
  part->headers = clist_new();
  part->sub_parts = clist_new();
  if ((part->languages == NULL) || (part->headers == NULL) ||
      (part->sub_parts == NULL)) {
    mailjmap_email_body_part_free(part);
    return NULL;
  }

  return part;
}

void mailjmap_email_body_part_free(struct mailjmap_email_body_part * part)
{
  clistiter * cur;

  if (part == NULL)
    return;

  free(part->part_id);
  free(part->blob_id);
  free(part->type);
  free(part->name);
  free(part->charset);
  free(part->disposition);
  free(part->cid);
  free(part->language);
  string_list_free(part->languages);
  free(part->location);
  email_header_list_free(part->headers);
  if (part->sub_parts != NULL) {
    for (cur = clist_begin(part->sub_parts); cur != NULL;
        cur = clist_next(cur))
      mailjmap_email_body_part_free(clist_content(cur));
    clist_free(part->sub_parts);
  }
  free(part);
}

static void email_body_part_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_body_part_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_email_body_value * mailjmap_email_body_value_new(void)
{
  struct mailjmap_email_body_value * body_value;

  body_value = malloc(sizeof(* body_value));
  if (body_value == NULL)
    return NULL;

  body_value->part_id = NULL;
  body_value->value = NULL;
  body_value->is_truncated = 0;
  return body_value;
}

void mailjmap_email_body_value_free(
    struct mailjmap_email_body_value * body_value)
{
  if (body_value == NULL)
    return;

  free(body_value->part_id);
  free(body_value->value);
  free(body_value);
}

static void email_body_value_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_body_value_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_email * mailjmap_email_new(void)
{
  struct mailjmap_email * email;

  email = malloc(sizeof(* email));
  if (email == NULL)
    return NULL;

  email->id = NULL;
  email->blob_id = NULL;
  email->thread_id = NULL;
  email->mailbox_ids = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYALL);
  email->keywords = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYALL);
  email->size = 0;
  email->received_at = NULL;
  email->message_id = NULL;
  email->in_reply_to = NULL;
  email->message_ids = clist_new();
  email->in_reply_to_list = clist_new();
  email->references = clist_new();
  email->sender = clist_new();
  email->from = clist_new();
  email->to = clist_new();
  email->cc = clist_new();
  email->bcc = clist_new();
  email->reply_to = clist_new();
  email->subject = NULL;
  email->sent_at = NULL;
  email->preview = NULL;
  email->headers = clist_new();
  email->body_structure = NULL;
  email->text_body = clist_new();
  email->html_body = clist_new();
  email->attachments = clist_new();
  email->body_values = clist_new();
  if ((email->mailbox_ids == NULL) || (email->keywords == NULL) ||
      (email->message_ids == NULL) || (email->in_reply_to_list == NULL) ||
      (email->references == NULL) || (email->sender == NULL) ||
      (email->from == NULL) || (email->to == NULL) ||
      (email->cc == NULL) || (email->bcc == NULL) ||
      (email->reply_to == NULL) || (email->headers == NULL) ||
      (email->text_body == NULL) ||
      (email->html_body == NULL) || (email->attachments == NULL) ||
      (email->body_values == NULL)) {
    mailjmap_email_free(email);
    return NULL;
  }

  return email;
}

void mailjmap_email_free(struct mailjmap_email * email)
{
  if (email == NULL)
    return;

  free(email->id);
  free(email->blob_id);
  free(email->thread_id);
  if (email->mailbox_ids != NULL)
    chash_free(email->mailbox_ids);
  if (email->keywords != NULL)
    chash_free(email->keywords);
  free(email->received_at);
  free(email->message_id);
  free(email->in_reply_to);
  string_list_free(email->message_ids);
  string_list_free(email->in_reply_to_list);
  string_list_free(email->references);
  email_address_list_free(email->sender);
  email_address_list_free(email->from);
  email_address_list_free(email->to);
  email_address_list_free(email->cc);
  email_address_list_free(email->bcc);
  email_address_list_free(email->reply_to);
  free(email->subject);
  free(email->sent_at);
  free(email->preview);
  email_header_list_free(email->headers);
  mailjmap_email_body_part_free(email->body_structure);
  email_body_part_list_free(email->text_body);
  email_body_part_list_free(email->html_body);
  email_body_part_list_free(email->attachments);
  email_body_value_list_free(email->body_values);
  free(email);
}

static void mailbox_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_mailbox_free(clist_content(cur));
  clist_free(list);
}

static void thread_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_thread_free(clist_content(cur));
  clist_free(list);
}

static void email_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_identity * mailjmap_identity_new(void)
{
  struct mailjmap_identity * identity;

  identity = malloc(sizeof(* identity));
  if (identity == NULL)
    return NULL;

  identity->id = NULL;
  identity->name = NULL;
  identity->email = NULL;
  identity->reply_to = NULL;
  identity->bcc = NULL;
  identity->text_signature = NULL;
  identity->html_signature = NULL;
  return identity;
}

void mailjmap_identity_free(struct mailjmap_identity * identity)
{
  if (identity == NULL)
    return;

  free(identity->id);
  free(identity->name);
  free(identity->email);
  free(identity->reply_to);
  free(identity->bcc);
  free(identity->text_signature);
  free(identity->html_signature);
  free(identity);
}

static void identity_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_identity_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_mailbox_get_result *
mailjmap_mailbox_get_result_new(void)
{
  struct mailjmap_mailbox_get_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->state = NULL;
  result->list = clist_new();
  result->not_found = clist_new();
  if ((result->list == NULL) || (result->not_found == NULL)) {
    mailjmap_mailbox_get_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_mailbox_get_result_free(
    struct mailjmap_mailbox_get_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  free(result->state);
  mailbox_list_free(result->list);
  string_list_free(result->not_found);
  free(result);
}

struct mailjmap_thread_get_result *
mailjmap_thread_get_result_new(void)
{
  struct mailjmap_thread_get_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->state = NULL;
  result->list = clist_new();
  result->not_found = clist_new();
  if ((result->list == NULL) || (result->not_found == NULL)) {
    mailjmap_thread_get_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_thread_get_result_free(
    struct mailjmap_thread_get_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  free(result->state);
  thread_list_free(result->list);
  string_list_free(result->not_found);
  free(result);
}

struct mailjmap_email_get_result *
mailjmap_email_get_result_new(void)
{
  struct mailjmap_email_get_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->state = NULL;
  result->list = clist_new();
  result->not_found = clist_new();
  if ((result->list == NULL) || (result->not_found == NULL)) {
    mailjmap_email_get_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_email_get_result_free(
    struct mailjmap_email_get_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  free(result->state);
  email_list_free(result->list);
  string_list_free(result->not_found);
  free(result);
}

struct mailjmap_identity_get_result *
mailjmap_identity_get_result_new(void)
{
  struct mailjmap_identity_get_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->state = NULL;
  result->list = clist_new();
  result->not_found = clist_new();
  if ((result->list == NULL) || (result->not_found == NULL)) {
    mailjmap_identity_get_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_identity_get_result_free(
    struct mailjmap_identity_get_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  free(result->state);
  identity_list_free(result->list);
  string_list_free(result->not_found);
  free(result);
}

struct mailjmap_changes_result * mailjmap_changes_result_new(void)
{
  struct mailjmap_changes_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->old_state = NULL;
  result->new_state = NULL;
  result->has_more_changes = 0;
  result->created = clist_new();
  result->updated = clist_new();
  result->destroyed = clist_new();
  if ((result->created == NULL) || (result->updated == NULL) ||
      (result->destroyed == NULL)) {
    mailjmap_changes_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_changes_result_free(struct mailjmap_changes_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  free(result->old_state);
  free(result->new_state);
  string_list_free(result->created);
  string_list_free(result->updated);
  string_list_free(result->destroyed);
  free(result);
}

struct mailjmap_query_result * mailjmap_query_result_new(void)
{
  struct mailjmap_query_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->query_state = NULL;
  result->can_calculate_changes = 0;
  result->position = 0;
  result->total = 0;
  result->has_total = 0;
  result->ids = clist_new();
  if (result->ids == NULL) {
    mailjmap_query_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_query_result_free(struct mailjmap_query_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  free(result->query_state);
  string_list_free(result->ids);
  free(result);
}

struct mailjmap_query_change * mailjmap_query_change_new(void)
{
  struct mailjmap_query_change * change;

  change = malloc(sizeof(* change));
  if (change == NULL)
    return NULL;

  change->id = NULL;
  change->index = 0;
  return change;
}

void mailjmap_query_change_free(struct mailjmap_query_change * change)
{
  if (change == NULL)
    return;

  free(change->id);
  free(change);
}

static void query_change_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_query_change_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_query_changes_result *
mailjmap_query_changes_result_new(void)
{
  struct mailjmap_query_changes_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->old_query_state = NULL;
  result->new_query_state = NULL;
  result->total = 0;
  result->has_total = 0;
  result->removed = clist_new();
  result->added = clist_new();
  if ((result->removed == NULL) || (result->added == NULL)) {
    mailjmap_query_changes_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_query_changes_result_free(
    struct mailjmap_query_changes_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  free(result->old_query_state);
  free(result->new_query_state);
  string_list_free(result->removed);
  query_change_list_free(result->added);
  free(result);
}

struct mailjmap_email_query_sort_comparator *
mailjmap_email_query_sort_comparator_new(const char * property)
{
  struct mailjmap_email_query_sort_comparator * comparator;

  if ((property == NULL) || (* property == '\0'))
    return NULL;

  comparator = malloc(sizeof(* comparator));
  if (comparator == NULL)
    return NULL;

  comparator->property = strdup(property);
  if (comparator->property == NULL) {
    free(comparator);
    return NULL;
  }
  comparator->has_is_ascending = 0;
  comparator->is_ascending = 0;
  comparator->collation = NULL;
  return comparator;
}

void mailjmap_email_query_sort_comparator_free(
    struct mailjmap_email_query_sort_comparator * comparator)
{
  if (comparator == NULL)
    return;

  free(comparator->property);
  free(comparator->collation);
  free(comparator);
}

void mailjmap_email_query_sort_comparator_set_is_ascending(
    struct mailjmap_email_query_sort_comparator * comparator,
    int is_ascending)
{
  if (comparator == NULL)
    return;

  comparator->has_is_ascending = 1;
  comparator->is_ascending = is_ascending;
}

int mailjmap_email_query_sort_comparator_set_collation(
    struct mailjmap_email_query_sort_comparator * comparator,
    const char * collation)
{
  char * copy;

  if (comparator == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  copy = NULL;
  if (collation != NULL) {
    copy = strdup(collation);
    if (copy == NULL)
      return MAILJMAP_ERROR_MEMORY;
  }

  free(comparator->collation);
  comparator->collation = copy;
  return MAILJMAP_NO_ERROR;
}

static int filter_operator_is_valid(const char * filter_operator)
{
  return (filter_operator != NULL) &&
      ((strcmp(filter_operator, "AND") == 0) ||
       (strcmp(filter_operator, "OR") == 0) ||
       (strcmp(filter_operator, "NOT") == 0));
}

struct mailjmap_email_query_filter *
mailjmap_email_query_filter_condition_new(void)
{
  struct mailjmap_email_query_filter * filter;

  filter = malloc(sizeof(* filter));
  if (filter == NULL)
    return NULL;

  filter->is_operator = 0;
  filter->filter_operator = NULL;
  filter->conditions = NULL;
  filter->in_mailbox = NULL;
  filter->before = NULL;
  filter->after = NULL;
  filter->has_min_size = 0;
  filter->min_size = 0;
  filter->has_max_size = 0;
  filter->max_size = 0;
  filter->has_keyword = NULL;
  filter->not_keyword = NULL;
  filter->text = NULL;
  filter->from = NULL;
  filter->to = NULL;
  filter->cc = NULL;
  filter->bcc = NULL;
  filter->subject = NULL;
  filter->body = NULL;
  return filter;
}

struct mailjmap_email_query_filter *
mailjmap_email_query_filter_operator_new(const char * filter_operator)
{
  struct mailjmap_email_query_filter * filter;

  if (!filter_operator_is_valid(filter_operator))
    return NULL;

  filter = mailjmap_email_query_filter_condition_new();
  if (filter == NULL)
    return NULL;

  filter->filter_operator = strdup(filter_operator);
  filter->conditions = clist_new();
  if ((filter->filter_operator == NULL) || (filter->conditions == NULL)) {
    mailjmap_email_query_filter_free(filter);
    return NULL;
  }
  filter->is_operator = 1;
  return filter;
}

static void email_query_filter_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_query_filter_free(clist_content(cur));
  clist_free(list);
}

void mailjmap_email_query_filter_free(
    struct mailjmap_email_query_filter * filter)
{
  if (filter == NULL)
    return;

  free(filter->filter_operator);
  email_query_filter_list_free(filter->conditions);
  free(filter->in_mailbox);
  free(filter->before);
  free(filter->after);
  free(filter->has_keyword);
  free(filter->not_keyword);
  free(filter->text);
  free(filter->from);
  free(filter->to);
  free(filter->cc);
  free(filter->bcc);
  free(filter->subject);
  free(filter->body);
  free(filter);
}

int mailjmap_email_query_filter_add_condition(
    struct mailjmap_email_query_filter * filter,
    struct mailjmap_email_query_filter * condition)
{
  if ((filter == NULL) || !filter->is_operator ||
      (filter->conditions == NULL) || (condition == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  if (clist_append(filter->conditions, condition) < 0)
    return MAILJMAP_ERROR_MEMORY;

  return MAILJMAP_NO_ERROR;
}

int mailjmap_email_query_filter_set_in_mailbox(
    struct mailjmap_email_query_filter * filter,
    const char * in_mailbox)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->in_mailbox, in_mailbox);
}

int mailjmap_email_query_filter_set_before(
    struct mailjmap_email_query_filter * filter,
    const char * before)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->before, before);
}

int mailjmap_email_query_filter_set_after(
    struct mailjmap_email_query_filter * filter,
    const char * after)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->after, after);
}

void mailjmap_email_query_filter_set_min_size(
    struct mailjmap_email_query_filter * filter,
    int min_size)
{
  if ((filter == NULL) || filter->is_operator)
    return;
  filter->has_min_size = 1;
  filter->min_size = min_size;
}

void mailjmap_email_query_filter_set_max_size(
    struct mailjmap_email_query_filter * filter,
    int max_size)
{
  if ((filter == NULL) || filter->is_operator)
    return;
  filter->has_max_size = 1;
  filter->max_size = max_size;
}

int mailjmap_email_query_filter_set_has_keyword(
    struct mailjmap_email_query_filter * filter,
    const char * has_keyword)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->has_keyword, has_keyword);
}

int mailjmap_email_query_filter_set_not_keyword(
    struct mailjmap_email_query_filter * filter,
    const char * not_keyword)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->not_keyword, not_keyword);
}

int mailjmap_email_query_filter_set_text(
    struct mailjmap_email_query_filter * filter,
    const char * text)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->text, text);
}

int mailjmap_email_query_filter_set_from(
    struct mailjmap_email_query_filter * filter,
    const char * from)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->from, from);
}

int mailjmap_email_query_filter_set_to(
    struct mailjmap_email_query_filter * filter,
    const char * to)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->to, to);
}

int mailjmap_email_query_filter_set_cc(
    struct mailjmap_email_query_filter * filter,
    const char * cc)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->cc, cc);
}

int mailjmap_email_query_filter_set_bcc(
    struct mailjmap_email_query_filter * filter,
    const char * bcc)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->bcc, bcc);
}

int mailjmap_email_query_filter_set_subject(
    struct mailjmap_email_query_filter * filter,
    const char * subject)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->subject, subject);
}

int mailjmap_email_query_filter_set_body(
    struct mailjmap_email_query_filter * filter,
    const char * body)
{
  if ((filter == NULL) || filter->is_operator)
    return MAILJMAP_ERROR_BAD_STATE;
  return replace_string(&filter->body, body);
}

struct mailjmap_import_created * mailjmap_import_created_new(void)
{
  struct mailjmap_import_created * created;

  created = malloc(sizeof(* created));
  if (created == NULL)
    return NULL;

  created->creation_id = NULL;
  created->email = NULL;
  return created;
}

void mailjmap_import_created_free(
    struct mailjmap_import_created * created)
{
  if (created == NULL)
    return;

  free(created->creation_id);
  mailjmap_email_free(created->email);
  free(created);
}

struct mailjmap_import_not_created *
mailjmap_import_not_created_new(void)
{
  struct mailjmap_import_not_created * not_created;

  not_created = malloc(sizeof(* not_created));
  if (not_created == NULL)
    return NULL;

  not_created->creation_id = NULL;
  not_created->type = NULL;
  not_created->description = NULL;
  return not_created;
}

void mailjmap_import_not_created_free(
    struct mailjmap_import_not_created * not_created)
{
  if (not_created == NULL)
    return;

  free(not_created->creation_id);
  free(not_created->type);
  free(not_created->description);
  free(not_created);
}

static void import_created_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_import_created_free(clist_content(cur));
  clist_free(list);
}

static void import_not_created_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_import_not_created_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_import_result * mailjmap_import_result_new(void)
{
  struct mailjmap_import_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->old_state = NULL;
  result->new_state = NULL;
  result->created = clist_new();
  result->not_created = clist_new();
  if ((result->created == NULL) || (result->not_created == NULL)) {
    mailjmap_import_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_import_result_free(struct mailjmap_import_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  free(result->old_state);
  free(result->new_state);
  import_created_list_free(result->created);
  import_not_created_list_free(result->not_created);
  free(result);
}

struct mailjmap_email_parse_item *
mailjmap_email_parse_item_new(void)
{
  struct mailjmap_email_parse_item * item;

  item = malloc(sizeof(* item));
  if (item == NULL)
    return NULL;

  item->blob_id = NULL;
  item->email = NULL;
  return item;
}

void mailjmap_email_parse_item_free(
    struct mailjmap_email_parse_item * item)
{
  if (item == NULL)
    return;

  free(item->blob_id);
  mailjmap_email_free(item->email);
  free(item);
}

static void email_parse_item_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_parse_item_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_email_parse_result *
mailjmap_email_parse_result_new(void)
{
  struct mailjmap_email_parse_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->parsed = clist_new();
  result->not_parsable = clist_new();
  if ((result->parsed == NULL) || (result->not_parsable == NULL)) {
    mailjmap_email_parse_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_email_parse_result_free(
    struct mailjmap_email_parse_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  email_parse_item_list_free(result->parsed);
  string_list_free(result->not_parsable);
  free(result);
}

struct mailjmap_search_snippet * mailjmap_search_snippet_new(void)
{
  struct mailjmap_search_snippet * snippet;

  snippet = malloc(sizeof(* snippet));
  if (snippet == NULL)
    return NULL;

  snippet->email_id = NULL;
  snippet->subject = NULL;
  snippet->preview = NULL;
  return snippet;
}

void mailjmap_search_snippet_free(
    struct mailjmap_search_snippet * snippet)
{
  if (snippet == NULL)
    return;

  free(snippet->email_id);
  free(snippet->subject);
  free(snippet->preview);
  free(snippet);
}

static void search_snippet_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_search_snippet_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_search_snippet_get_result *
mailjmap_search_snippet_get_result_new(void)
{
  struct mailjmap_search_snippet_get_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->list = clist_new();
  result->not_found = clist_new();
  if ((result->list == NULL) || (result->not_found == NULL)) {
    mailjmap_search_snippet_get_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_search_snippet_get_result_free(
    struct mailjmap_search_snippet_get_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  search_snippet_list_free(result->list);
  string_list_free(result->not_found);
  free(result);
}

static int replace_string(char ** target, const char * value)
{
  char * copy;

  if (target == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  copy = NULL;
  if (value != NULL) {
    copy = strdup(value);
    if (copy == NULL)
      return MAILJMAP_ERROR_MEMORY;
  }

  free(* target);
  * target = copy;
  return MAILJMAP_NO_ERROR;
}

struct mailjmap_mailbox_set_item *
mailjmap_mailbox_set_item_new(const char * id)
{
  struct mailjmap_mailbox_set_item * item;

  item = malloc(sizeof(* item));
  if (item == NULL)
    return NULL;

  item->id = NULL;
  item->name = NULL;
  item->has_parent_id = 0;
  item->parent_id_is_null = 0;
  item->parent_id = NULL;
  item->has_role = 0;
  item->role_is_null = 0;
  item->role = NULL;
  item->has_sort_order = 0;
  item->sort_order = 0;
  item->has_is_subscribed = 0;
  item->is_subscribed = 0;

  if (id != NULL) {
    item->id = strdup(id);
    if (item->id == NULL) {
      mailjmap_mailbox_set_item_free(item);
      return NULL;
    }
  }

  return item;
}

void mailjmap_mailbox_set_item_free(
    struct mailjmap_mailbox_set_item * item)
{
  if (item == NULL)
    return;

  free(item->id);
  free(item->name);
  free(item->parent_id);
  free(item->role);
  free(item);
}

int mailjmap_mailbox_set_item_set_name(
    struct mailjmap_mailbox_set_item * item, const char * name)
{
  if (item == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&item->name, name);
}

int mailjmap_mailbox_set_item_set_parent_id(
    struct mailjmap_mailbox_set_item * item, const char * parent_id)
{
  int r;

  if ((item == NULL) || (parent_id == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  r = replace_string(&item->parent_id, parent_id);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  item->has_parent_id = 1;
  item->parent_id_is_null = 0;
  return MAILJMAP_NO_ERROR;
}

void mailjmap_mailbox_set_item_set_parent_id_null(
    struct mailjmap_mailbox_set_item * item)
{
  if (item == NULL)
    return;

  free(item->parent_id);
  item->parent_id = NULL;
  item->has_parent_id = 1;
  item->parent_id_is_null = 1;
}

int mailjmap_mailbox_set_item_set_role(
    struct mailjmap_mailbox_set_item * item, const char * role)
{
  int r;

  if ((item == NULL) || (role == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  r = replace_string(&item->role, role);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  item->has_role = 1;
  item->role_is_null = 0;
  return MAILJMAP_NO_ERROR;
}

void mailjmap_mailbox_set_item_set_role_null(
    struct mailjmap_mailbox_set_item * item)
{
  if (item == NULL)
    return;

  free(item->role);
  item->role = NULL;
  item->has_role = 1;
  item->role_is_null = 1;
}

void mailjmap_mailbox_set_item_set_sort_order(
    struct mailjmap_mailbox_set_item * item, int sort_order)
{
  if (item == NULL)
    return;

  item->has_sort_order = 1;
  item->sort_order = sort_order;
}

void mailjmap_mailbox_set_item_set_is_subscribed(
    struct mailjmap_mailbox_set_item * item, int is_subscribed)
{
  if (item == NULL)
    return;

  item->has_is_subscribed = 1;
  item->is_subscribed = is_subscribed ? 1 : 0;
}

struct mailjmap_email_set_item *
mailjmap_email_set_item_new(const char * id)
{
  struct mailjmap_email_set_item * item;

  item = malloc(sizeof(* item));
  if (item == NULL)
    return NULL;

  item->id = NULL;
  item->has_mailbox_ids = 0;
  item->mailbox_ids = clist_new();
  item->has_keywords = 0;
  item->keywords = clist_new();
  if ((item->mailbox_ids == NULL) || (item->keywords == NULL)) {
    mailjmap_email_set_item_free(item);
    return NULL;
  }

  if (id != NULL) {
    item->id = strdup(id);
    if (item->id == NULL) {
      mailjmap_email_set_item_free(item);
      return NULL;
    }
  }

  return item;
}

void mailjmap_email_set_item_free(
    struct mailjmap_email_set_item * item)
{
  if (item == NULL)
    return;

  free(item->id);
  string_list_free(item->mailbox_ids);
  string_list_free(item->keywords);
  free(item);
}

static int email_set_item_add_string(clist * list, const char * string)
{
  char * copy;

  if ((list == NULL) || (string == NULL) || (* string == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;

  copy = strdup(string);
  if (copy == NULL)
    return MAILJMAP_ERROR_MEMORY;
  if (clist_append(list, copy) < 0) {
    free(copy);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

int mailjmap_email_set_item_add_mailbox_id(
    struct mailjmap_email_set_item * item, const char * mailbox_id)
{
  int r;

  if (item == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  r = email_set_item_add_string(item->mailbox_ids, mailbox_id);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  item->has_mailbox_ids = 1;
  return MAILJMAP_NO_ERROR;
}

int mailjmap_email_set_item_add_keyword(
    struct mailjmap_email_set_item * item, const char * keyword)
{
  int r;

  if (item == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  r = email_set_item_add_string(item->keywords, keyword);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  item->has_keywords = 1;
  return MAILJMAP_NO_ERROR;
}

struct mailjmap_email_copy_item *
mailjmap_email_copy_item_new(const char * creation_id, const char * id)
{
  struct mailjmap_email_copy_item * item;

  if ((creation_id == NULL) || (* creation_id == '\0') ||
      (id == NULL) || (* id == '\0'))
    return NULL;

  item = malloc(sizeof(* item));
  if (item == NULL)
    return NULL;

  item->creation_id = NULL;
  item->id = NULL;
  item->has_mailbox_ids = 0;
  item->mailbox_ids = clist_new();
  item->has_keywords = 0;
  item->keywords = clist_new();
  item->received_at = NULL;
  if ((item->mailbox_ids == NULL) || (item->keywords == NULL)) {
    mailjmap_email_copy_item_free(item);
    return NULL;
  }

  item->creation_id = strdup(creation_id);
  item->id = strdup(id);
  if ((item->creation_id == NULL) || (item->id == NULL)) {
    mailjmap_email_copy_item_free(item);
    return NULL;
  }

  return item;
}

void mailjmap_email_copy_item_free(
    struct mailjmap_email_copy_item * item)
{
  if (item == NULL)
    return;

  free(item->creation_id);
  free(item->id);
  string_list_free(item->mailbox_ids);
  string_list_free(item->keywords);
  free(item->received_at);
  free(item);
}

int mailjmap_email_copy_item_add_mailbox_id(
    struct mailjmap_email_copy_item * item, const char * mailbox_id)
{
  int r;

  if (item == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  r = email_set_item_add_string(item->mailbox_ids, mailbox_id);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  item->has_mailbox_ids = 1;
  return MAILJMAP_NO_ERROR;
}

int mailjmap_email_copy_item_add_keyword(
    struct mailjmap_email_copy_item * item, const char * keyword)
{
  int r;

  if (item == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  r = email_set_item_add_string(item->keywords, keyword);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  item->has_keywords = 1;
  return MAILJMAP_NO_ERROR;
}

int mailjmap_email_copy_item_set_received_at(
    struct mailjmap_email_copy_item * item, const char * received_at)
{
  if ((item == NULL) || (received_at == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&item->received_at, received_at);
}

struct mailjmap_email_submission_set_item *
mailjmap_email_submission_set_item_new(const char * id)
{
  struct mailjmap_email_submission_set_item * item;

  item = malloc(sizeof(* item));
  if (item == NULL)
    return NULL;

  item->id = NULL;
  item->email_id = NULL;
  item->identity_id = NULL;
  item->send_at = NULL;
  item->undo_status = NULL;
  item->envelope_mail_from = NULL;
  item->envelope_rcpt_to = clist_new();
  if (item->envelope_rcpt_to == NULL) {
    mailjmap_email_submission_set_item_free(item);
    return NULL;
  }

  if (id != NULL) {
    item->id = strdup(id);
    if (item->id == NULL) {
      mailjmap_email_submission_set_item_free(item);
      return NULL;
    }
  }

  return item;
}

void mailjmap_email_submission_set_item_free(
    struct mailjmap_email_submission_set_item * item)
{
  if (item == NULL)
    return;

  free(item->id);
  free(item->email_id);
  free(item->identity_id);
  free(item->send_at);
  free(item->undo_status);
  mailjmap_email_address_free(item->envelope_mail_from);
  email_address_list_free(item->envelope_rcpt_to);
  free(item);
}

int mailjmap_email_submission_set_item_set_email_id(
    struct mailjmap_email_submission_set_item * item,
    const char * email_id)
{
  if ((item == NULL) || (email_id == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&item->email_id, email_id);
}

int mailjmap_email_submission_set_item_set_identity_id(
    struct mailjmap_email_submission_set_item * item,
    const char * identity_id)
{
  if ((item == NULL) || (identity_id == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&item->identity_id, identity_id);
}

int mailjmap_email_submission_set_item_set_send_at(
    struct mailjmap_email_submission_set_item * item,
    const char * send_at)
{
  if ((item == NULL) || (send_at == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&item->send_at, send_at);
}

int mailjmap_email_submission_set_item_set_undo_status(
    struct mailjmap_email_submission_set_item * item,
    const char * undo_status)
{
  if ((item == NULL) || (undo_status == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&item->undo_status, undo_status);
}

static struct mailjmap_email_address * email_address_new_with_values(
    const char * name, const char * email)
{
  struct mailjmap_email_address * address;

  if ((email == NULL) || (* email == '\0'))
    return NULL;

  address = mailjmap_email_address_new();
  if (address == NULL)
    return NULL;

  if (name != NULL) {
    address->name = strdup(name);
    if (address->name == NULL) {
      mailjmap_email_address_free(address);
      return NULL;
    }
  }
  address->email = strdup(email);
  if (address->email == NULL) {
    mailjmap_email_address_free(address);
    return NULL;
  }

  return address;
}

int mailjmap_email_submission_set_item_set_envelope_mail_from(
    struct mailjmap_email_submission_set_item * item,
    const char * name,
    const char * email)
{
  struct mailjmap_email_address * address;

  if ((item == NULL) || (email == NULL) || (* email == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;

  address = email_address_new_with_values(name, email);
  if (address == NULL)
    return MAILJMAP_ERROR_MEMORY;

  mailjmap_email_address_free(item->envelope_mail_from);
  item->envelope_mail_from = address;
  return MAILJMAP_NO_ERROR;
}

int mailjmap_email_submission_set_item_add_envelope_rcpt_to(
    struct mailjmap_email_submission_set_item * item,
    const char * name,
    const char * email)
{
  struct mailjmap_email_address * address;

  if ((item == NULL) || (item->envelope_rcpt_to == NULL) ||
      (email == NULL) || (* email == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;

  address = email_address_new_with_values(name, email);
  if (address == NULL)
    return MAILJMAP_ERROR_MEMORY;

  if (clist_append(item->envelope_rcpt_to, address) < 0) {
    mailjmap_email_address_free(address);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

struct mailjmap_email_submission_delivery_status *
mailjmap_email_submission_delivery_status_new(void)
{
  struct mailjmap_email_submission_delivery_status * status;

  status = malloc(sizeof(* status));
  if (status == NULL)
    return NULL;

  status->email = NULL;
  status->smtp_reply = NULL;
  status->delivered = NULL;
  status->displayed = NULL;
  return status;
}

void mailjmap_email_submission_delivery_status_free(
    struct mailjmap_email_submission_delivery_status * status)
{
  if (status == NULL)
    return;

  free(status->email);
  free(status->smtp_reply);
  free(status->delivered);
  free(status->displayed);
  free(status);
}

static void email_submission_delivery_status_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_submission_delivery_status_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_set_created * mailjmap_set_created_new(void)
{
  struct mailjmap_set_created * created;

  created = malloc(sizeof(* created));
  if (created == NULL)
    return NULL;

  created->creation_id = NULL;
  created->id = NULL;
  created->email_id = NULL;
  created->identity_id = NULL;
  created->thread_id = NULL;
  created->send_at = NULL;
  created->undo_status = NULL;
  created->envelope_mail_from = NULL;
  created->envelope_rcpt_to = clist_new();
  created->delivery_status = clist_new();
  created->dsn_blob_ids = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYALL);
  if ((created->envelope_rcpt_to == NULL) ||
      (created->delivery_status == NULL) ||
      (created->dsn_blob_ids == NULL)) {
    mailjmap_set_created_free(created);
    return NULL;
  }
  return created;
}

void mailjmap_set_created_free(struct mailjmap_set_created * created)
{
  if (created == NULL)
    return;

  free(created->creation_id);
  free(created->id);
  free(created->email_id);
  free(created->identity_id);
  free(created->thread_id);
  free(created->send_at);
  free(created->undo_status);
  mailjmap_email_address_free(created->envelope_mail_from);
  email_address_list_free(created->envelope_rcpt_to);
  email_submission_delivery_status_list_free(created->delivery_status);
  if (created->dsn_blob_ids != NULL)
    chash_free(created->dsn_blob_ids);
  free(created);
}

static void set_created_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_set_created_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_set_error * mailjmap_set_error_new(void)
{
  struct mailjmap_set_error * error;

  error = malloc(sizeof(* error));
  if (error == NULL)
    return NULL;

  error->id = NULL;
  error->type = NULL;
  error->description = NULL;
  return error;
}

void mailjmap_set_error_free(struct mailjmap_set_error * error)
{
  if (error == NULL)
    return;

  free(error->id);
  free(error->type);
  free(error->description);
  free(error);
}

static void set_error_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_set_error_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_set_result * mailjmap_set_result_new(void)
{
  struct mailjmap_set_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->account_id = NULL;
  result->old_state = NULL;
  result->new_state = NULL;
  result->created = clist_new();
  result->updated = clist_new();
  result->destroyed = clist_new();
  result->not_created = clist_new();
  result->not_updated = clist_new();
  result->not_destroyed = clist_new();
  if ((result->created == NULL) || (result->updated == NULL) ||
      (result->destroyed == NULL) || (result->not_created == NULL) ||
      (result->not_updated == NULL) || (result->not_destroyed == NULL)) {
    mailjmap_set_result_free(result);
    return NULL;
  }

  return result;
}

void mailjmap_set_result_free(struct mailjmap_set_result * result)
{
  if (result == NULL)
    return;

  free(result->account_id);
  free(result->old_state);
  free(result->new_state);
  set_created_list_free(result->created);
  string_list_free(result->updated);
  string_list_free(result->destroyed);
  set_error_list_free(result->not_created);
  set_error_list_free(result->not_updated);
  set_error_list_free(result->not_destroyed);
  free(result);
}

static int json_array_from_string_list(clist * list,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * array;
  clistiter * cur;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  array = NULL;
  r = mailjmap_json_new_array(&array);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    mailjmap_json_value * value;
    const char * string;

    value = NULL;
    string = clist_content(cur);
    if (string == NULL) {
      r = MAILJMAP_ERROR_BAD_STATE;
      goto err;
    }

    r = mailjmap_json_new_string(string, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_array_append_new(array, value);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(value);
      goto err;
    }
  }

  * result = array;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(array);
  return r;
}

static int object_set_string(mailjmap_json_value * object,
    const char * key, const char * string)
{
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = mailjmap_json_new_string(string, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = mailjmap_json_object_set_new(object, key, value);
  if (r != MAILJMAP_NO_ERROR) {
    mailjmap_json_free(value);
    return r;
  }

  return MAILJMAP_NO_ERROR;
}

static int object_set_integer(mailjmap_json_value * object,
    const char * key, int integer)
{
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = mailjmap_json_new_integer(integer, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = mailjmap_json_object_set_new(object, key, value);
  if (r != MAILJMAP_NO_ERROR) {
    mailjmap_json_free(value);
    return r;
  }

  return MAILJMAP_NO_ERROR;
}

static int object_set_boolean(mailjmap_json_value * object,
    const char * key, int boolean)
{
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = mailjmap_json_new_boolean(boolean, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = mailjmap_json_object_set_new(object, key, value);
  if (r != MAILJMAP_NO_ERROR) {
    mailjmap_json_free(value);
    return r;
  }

  return MAILJMAP_NO_ERROR;
}

static int object_set_null(mailjmap_json_value * object, const char * key)
{
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = mailjmap_json_new_null(&value);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = mailjmap_json_object_set_new(object, key, value);
  if (r != MAILJMAP_NO_ERROR) {
    mailjmap_json_free(value);
    return r;
  }

  return MAILJMAP_NO_ERROR;
}

static int json_true_object_from_string_list(clist * list,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  clistiter * cur;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    const char * string;

    string = clist_content(cur);
    if (string == NULL) {
      r = MAILJMAP_ERROR_BAD_STATE;
      goto err;
    }

    r = object_set_boolean(object, string, 1);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(object);
  return r;
}

static int build_mailbox_get_arguments(const char * account_id,
    clist * ids, clist * properties, mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * value;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  value = NULL;

  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  if (ids != NULL) {
    r = json_array_from_string_list(ids, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "ids", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  if (properties != NULL) {
    r = json_array_from_string_list(properties, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "properties", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(root);
  return r;
}

static int json_from_email_query_filter(
    struct mailjmap_email_query_filter * filter,
    mailjmap_json_value ** result);

static int build_search_snippet_get_arguments(const char * account_id,
    clist * email_ids, const char * text,
    struct mailjmap_email_query_filter * filter,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * value;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') ||
      (email_ids == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  value = NULL;

  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = json_array_from_string_list(email_ids, &value);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_set_new(root, "emailIds", value);
  value = NULL;
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  if (filter != NULL) {
    r = json_from_email_query_filter(filter, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "filter", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  else if (text != NULL) {
    r = mailjmap_json_new_object(&value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = object_set_string(value, "text", text);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "filter", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(root);
  return r;
}

static int build_mailbox_changes_arguments(const char * account_id,
    const char * since_state, mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') ||
      (since_state == NULL) || (* since_state == '\0') || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_set_string(root, "sinceState", since_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(root);
  return r;
}

static int build_mailbox_query_arguments(const char * account_id,
    int position, int limit, mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (position >= 0) {
    r = object_set_integer(root, "position", position);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (limit >= 0) {
    r = object_set_integer(root, "limit", limit);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(root);
  return r;
}

static int object_set_text_filter(mailjmap_json_value * object,
    const char * text)
{
  mailjmap_json_value * filter;
  int r;

  if (text == NULL)
    return MAILJMAP_NO_ERROR;

  filter = NULL;
  r = mailjmap_json_new_object(&filter);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = object_set_string(filter, "text", text);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_set_new(object, "filter", filter);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(filter);
  return r;
}

static int json_array_from_email_query_sort_comparators(clist * sort,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * array;
  clistiter * cur;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  array = NULL;
  r = mailjmap_json_new_array(&array);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(sort); cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_email_query_sort_comparator * comparator;
    mailjmap_json_value * object;

    comparator = clist_content(cur);
    if ((comparator == NULL) || (comparator->property == NULL)) {
      r = MAILJMAP_ERROR_BAD_STATE;
      goto err;
    }

    object = NULL;
    r = mailjmap_json_new_object(&object);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = object_set_string(object, "property", comparator->property);
    if (r != MAILJMAP_NO_ERROR)
      goto object_err;
    if (comparator->has_is_ascending) {
      r = object_set_boolean(object, "isAscending",
          comparator->is_ascending);
      if (r != MAILJMAP_NO_ERROR)
        goto object_err;
    }
    if (comparator->collation != NULL) {
      r = object_set_string(object, "collation", comparator->collation);
      if (r != MAILJMAP_NO_ERROR)
        goto object_err;
    }

    r = mailjmap_json_array_append_new(array, object);
    if (r != MAILJMAP_NO_ERROR)
      goto object_err;
    continue;

   object_err:
    mailjmap_json_free(object);
    goto err;
  }

  * result = array;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(array);
  return r;
}

static int json_from_email_query_filter(
    struct mailjmap_email_query_filter * filter,
    mailjmap_json_value ** result);

static int json_array_from_email_query_filters(clist * filters,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * array;
  clistiter * cur;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  array = NULL;
  r = mailjmap_json_new_array(&array);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(filters); cur != NULL; cur = clist_next(cur)) {
    mailjmap_json_value * value;

    value = NULL;
    r = json_from_email_query_filter(clist_content(cur), &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_array_append_new(array, value);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(value);
      goto err;
    }
  }

  * result = array;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(array);
  return r;
}

static int json_from_email_query_filter_condition(
    struct mailjmap_email_query_filter * filter,
    mailjmap_json_value * object)
{
  int r;

  if (filter->in_mailbox != NULL) {
    r = object_set_string(object, "inMailbox", filter->in_mailbox);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->before != NULL) {
    r = object_set_string(object, "before", filter->before);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->after != NULL) {
    r = object_set_string(object, "after", filter->after);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->has_min_size) {
    r = object_set_integer(object, "minSize", filter->min_size);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->has_max_size) {
    r = object_set_integer(object, "maxSize", filter->max_size);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->has_keyword != NULL) {
    r = object_set_string(object, "hasKeyword", filter->has_keyword);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->not_keyword != NULL) {
    r = object_set_string(object, "notKeyword", filter->not_keyword);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->text != NULL) {
    r = object_set_string(object, "text", filter->text);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->from != NULL) {
    r = object_set_string(object, "from", filter->from);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->to != NULL) {
    r = object_set_string(object, "to", filter->to);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->cc != NULL) {
    r = object_set_string(object, "cc", filter->cc);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->bcc != NULL) {
    r = object_set_string(object, "bcc", filter->bcc);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->subject != NULL) {
    r = object_set_string(object, "subject", filter->subject);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (filter->body != NULL) {
    r = object_set_string(object, "body", filter->body);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }

  return MAILJMAP_NO_ERROR;
}

static int json_from_email_query_filter(
    struct mailjmap_email_query_filter * filter,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  mailjmap_json_value * conditions;
  int r;

  if ((filter == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  conditions = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  if (filter->is_operator) {
    if ((filter->filter_operator == NULL) || (filter->conditions == NULL)) {
      r = MAILJMAP_ERROR_BAD_STATE;
      goto err;
    }

    r = object_set_string(object, "operator", filter->filter_operator);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = json_array_from_email_query_filters(filter->conditions, &conditions);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, "conditions", conditions);
    conditions = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  else {
    r = json_from_email_query_filter_condition(filter, object);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(conditions);
  mailjmap_json_free(object);
  return r;
}

static int build_email_query_arguments(const char * account_id,
    const char * text, struct mailjmap_email_query_filter * filter,
    clist * sort, int position, const char * anchor, int anchor_offset,
    int limit, int calculate_total, int collapse_threads,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = build_mailbox_query_arguments(account_id, position, limit, &root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  if (filter != NULL) {
    r = json_from_email_query_filter(filter, &value);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(root);
      return r;
    }
    r = mailjmap_json_object_set_new(root, "filter", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(root);
      return r;
    }
  }
  else {
    r = object_set_text_filter(root, text);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(root);
      return r;
    }
  }
  if (sort != NULL) {
    r = json_array_from_email_query_sort_comparators(sort, &value);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(root);
      return r;
    }
    r = mailjmap_json_object_set_new(root, "sort", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(root);
      return r;
    }
  }
  if (anchor != NULL) {
    r = object_set_string(root, "anchor", anchor);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(root);
      return r;
    }
  }
  if (anchor_offset != 0) {
    r = object_set_integer(root, "anchorOffset", anchor_offset);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(root);
      return r;
    }
  }
  if (calculate_total >= 0) {
    r = object_set_boolean(root, "calculateTotal", calculate_total);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(root);
      return r;
    }
  }
  if (collapse_threads >= 0) {
    r = object_set_boolean(root, "collapseThreads", collapse_threads);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(root);
      return r;
    }
  }

  * result = root;
  return MAILJMAP_NO_ERROR;
}

static int build_query_changes_arguments(const char * account_id,
    const char * since_query_state, const char * text,
    struct mailjmap_email_query_filter * filter, clist * sort,
    int max_changes, const char * up_to_id, int calculate_total,
    int collapse_threads, mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * value;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') ||
      (since_query_state == NULL) || (* since_query_state == '\0') ||
      (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  value = NULL;
  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_set_string(root, "sinceQueryState", since_query_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (filter != NULL) {
    r = json_from_email_query_filter(filter, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "filter", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  else {
    r = object_set_text_filter(root, text);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (sort != NULL) {
    r = json_array_from_email_query_sort_comparators(sort, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "sort", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (max_changes >= 0) {
    r = object_set_integer(root, "maxChanges", max_changes);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (up_to_id != NULL) {
    r = object_set_string(root, "upToId", up_to_id);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (calculate_total >= 0) {
    r = object_set_boolean(root, "calculateTotal", calculate_total);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (collapse_threads >= 0) {
    r = object_set_boolean(root, "collapseThreads", collapse_threads);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(root);
  return r;
}

static int build_email_import_arguments(const char * account_id,
    const char * creation_id, const char * blob_id, clist * mailbox_ids,
    clist * keywords, const char * received_at,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * emails;
  mailjmap_json_value * import_object;
  mailjmap_json_value * value;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') ||
      (creation_id == NULL) || (* creation_id == '\0') ||
      (blob_id == NULL) || (* blob_id == '\0') || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  emails = NULL;
  import_object = NULL;
  value = NULL;

  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = mailjmap_json_new_object(&emails);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_new_object(&import_object);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_set_string(import_object, "blobId", blob_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  if (mailbox_ids != NULL) {
    r = json_true_object_from_string_list(mailbox_ids, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(import_object, "mailboxIds", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  if (keywords != NULL) {
    r = json_true_object_from_string_list(keywords, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(import_object, "keywords", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  if (received_at != NULL) {
    r = object_set_string(import_object, "receivedAt", received_at);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  r = mailjmap_json_object_set_new(emails, creation_id, import_object);
  import_object = NULL;
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_set_new(root, "emails", emails);
  emails = NULL;
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(import_object);
  mailjmap_json_free(emails);
  mailjmap_json_free(root);
  return r;
}

static int build_email_parse_arguments(const char * account_id,
    clist * blob_ids, clist * properties, clist * body_properties,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * value;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') ||
      (blob_ids == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  value = NULL;

  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = json_array_from_string_list(blob_ids, &value);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_set_new(root, "blobIds", value);
  value = NULL;
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  if (properties != NULL) {
    r = json_array_from_string_list(properties, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "properties", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  if (body_properties != NULL) {
    r = json_array_from_string_list(body_properties, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "bodyProperties", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(root);
  return r;
}

static int json_from_mailbox_set_item(
    struct mailjmap_mailbox_set_item * item,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  int r;

  if ((item == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  if (item->name != NULL) {
    r = object_set_string(object, "name", item->name);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (item->has_parent_id) {
    if (item->parent_id_is_null)
      r = object_set_null(object, "parentId");
    else
      r = object_set_string(object, "parentId", item->parent_id);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (item->has_role) {
    if (item->role_is_null)
      r = object_set_null(object, "role");
    else
      r = object_set_string(object, "role", item->role);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (item->has_sort_order) {
    r = object_set_integer(object, "sortOrder", item->sort_order);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (item->has_is_subscribed) {
    r = object_set_boolean(object, "isSubscribed",
        item->is_subscribed);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(object);
  return r;
}

static int json_object_from_mailbox_set_item_list(clist * list,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  clistiter * cur;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_mailbox_set_item * item;
    mailjmap_json_value * value;

    value = NULL;
    item = clist_content(cur);
    if ((item == NULL) || (item->id == NULL) || (* item->id == '\0')) {
      r = MAILJMAP_ERROR_BAD_STATE;
      goto err;
    }

    r = json_from_mailbox_set_item(item, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, item->id, value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(object);
  return r;
}

static int build_mailbox_set_arguments(const char * account_id,
    const char * if_in_state, clist * create, clist * update,
    clist * destroy, mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * value;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  value = NULL;
  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (if_in_state != NULL) {
    r = object_set_string(root, "ifInState", if_in_state);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (create != NULL) {
    r = json_object_from_mailbox_set_item_list(create, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "create", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (update != NULL) {
    r = json_object_from_mailbox_set_item_list(update, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "update", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (destroy != NULL) {
    r = json_array_from_string_list(destroy, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "destroy", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(root);
  return r;
}

static int json_from_email_set_item(
    struct mailjmap_email_set_item * item,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  mailjmap_json_value * value;
  int r;

  if ((item == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  value = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  if (item->has_mailbox_ids) {
    r = json_true_object_from_string_list(item->mailbox_ids, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, "mailboxIds", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (item->has_keywords) {
    r = json_true_object_from_string_list(item->keywords, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, "keywords", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(object);
  return r;
}

static int json_object_from_email_set_item_list(clist * list,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  clistiter * cur;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_email_set_item * item;
    mailjmap_json_value * value;

    value = NULL;
    item = clist_content(cur);
    if ((item == NULL) || (item->id == NULL) || (* item->id == '\0')) {
      r = MAILJMAP_ERROR_BAD_STATE;
      goto err;
    }

    r = json_from_email_set_item(item, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, item->id, value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(object);
  return r;
}

static int build_email_set_arguments(const char * account_id,
    const char * if_in_state, clist * create, clist * update,
    clist * destroy, mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * value;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  value = NULL;
  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (if_in_state != NULL) {
    r = object_set_string(root, "ifInState", if_in_state);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (create != NULL) {
    r = json_object_from_email_set_item_list(create, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "create", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (update != NULL) {
    r = json_object_from_email_set_item_list(update, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "update", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (destroy != NULL) {
    r = json_array_from_string_list(destroy, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "destroy", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(root);
  return r;
}

static int json_from_email_copy_item(
    struct mailjmap_email_copy_item * item,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  mailjmap_json_value * value;
  int r;

  if ((item == NULL) || (item->id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  value = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(object, "id", item->id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (item->has_mailbox_ids) {
    r = json_true_object_from_string_list(item->mailbox_ids, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, "mailboxIds", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (item->has_keywords) {
    r = json_true_object_from_string_list(item->keywords, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, "keywords", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (item->received_at != NULL) {
    r = object_set_string(object, "receivedAt", item->received_at);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(object);
  return r;
}

static int json_object_from_email_copy_item_list(clist * list,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  clistiter * cur;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_email_copy_item * item;
    mailjmap_json_value * value;

    value = NULL;
    item = clist_content(cur);
    if ((item == NULL) || (item->creation_id == NULL) ||
        (* item->creation_id == '\0')) {
      r = MAILJMAP_ERROR_BAD_STATE;
      goto err;
    }

    r = json_from_email_copy_item(item, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, item->creation_id, value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(object);
  return r;
}

static int json_from_email_address(struct mailjmap_email_address * address,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  int r;

  if ((address == NULL) || (address->email == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  if (address->name != NULL)
    r = object_set_string(object, "name", address->name);
  else
    r = object_set_null(object, "name");
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = object_set_string(object, "email", address->email);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(object);
  return r;
}

static int json_array_from_email_address_list(clist * list,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * array;
  clistiter * cur;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  array = NULL;
  r = mailjmap_json_new_array(&array);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    mailjmap_json_value * value;

    value = NULL;
    r = json_from_email_address(clist_content(cur), &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_array_append_new(array, value);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(value);
      goto err;
    }
  }

  * result = array;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(array);
  return r;
}

static int json_from_email_submission_envelope(
    struct mailjmap_email_submission_set_item * item,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  mailjmap_json_value * value;
  int r;

  if ((item == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  value = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  if (item->envelope_mail_from != NULL) {
    r = json_from_email_address(item->envelope_mail_from, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, "mailFrom", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if ((item->envelope_rcpt_to != NULL) &&
      (clist_begin(item->envelope_rcpt_to) != NULL)) {
    r = json_array_from_email_address_list(item->envelope_rcpt_to, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, "rcptTo", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(object);
  return r;
}

static int build_email_copy_arguments(const char * account_id,
    const char * from_account_id, const char * if_from_in_state,
    clist * create, int on_success_destroy_original,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * value;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') ||
      (from_account_id == NULL) || (* from_account_id == '\0') ||
      (create == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  value = NULL;
  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_set_string(root, "fromAccountId", from_account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (if_from_in_state != NULL) {
    r = object_set_string(root, "ifFromInState", if_from_in_state);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  r = json_object_from_email_copy_item_list(create, &value);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_set_new(root, "create", value);
  value = NULL;
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (on_success_destroy_original >= 0) {
    r = object_set_boolean(root, "onSuccessDestroyOriginal",
        on_success_destroy_original);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(root);
  return r;
}

static int json_from_email_submission_set_item(
    struct mailjmap_email_submission_set_item * item,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  int r;

  if ((item == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  if (item->email_id != NULL) {
    r = object_set_string(object, "emailId", item->email_id);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (item->identity_id != NULL) {
    r = object_set_string(object, "identityId", item->identity_id);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (item->send_at != NULL) {
    r = object_set_string(object, "sendAt", item->send_at);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (item->undo_status != NULL) {
    r = object_set_string(object, "undoStatus", item->undo_status);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if ((item->envelope_mail_from != NULL) ||
      ((item->envelope_rcpt_to != NULL) &&
      (clist_begin(item->envelope_rcpt_to) != NULL))) {
    mailjmap_json_value * envelope;

    envelope = NULL;
    r = json_from_email_submission_envelope(item, &envelope);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, "envelope", envelope);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(envelope);
      goto err;
    }
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(object);
  return r;
}

static int json_object_from_email_submission_set_item_list(clist * list,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * object;
  clistiter * cur;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  object = NULL;
  r = mailjmap_json_new_object(&object);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_email_submission_set_item * item;
    mailjmap_json_value * value;

    value = NULL;
    item = clist_content(cur);
    if ((item == NULL) || (item->id == NULL) || (* item->id == '\0')) {
      r = MAILJMAP_ERROR_BAD_STATE;
      goto err;
    }

    r = json_from_email_submission_set_item(item, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(object, item->id, value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = object;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(object);
  return r;
}

static int build_email_submission_set_arguments(const char * account_id,
    const char * if_in_state, clist * create, clist * update,
    clist * destroy, mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * value;
  int r;

  if ((account_id == NULL) || (* account_id == '\0') || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  value = NULL;
  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(root, "accountId", account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (if_in_state != NULL) {
    r = object_set_string(root, "ifInState", if_in_state);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (create != NULL) {
    r = json_object_from_email_submission_set_item_list(create, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "create", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (update != NULL) {
    r = json_object_from_email_submission_set_item_list(update, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "update", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }
  if (destroy != NULL) {
    r = json_array_from_string_list(destroy, &value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
    r = mailjmap_json_object_set_new(root, "destroy", value);
    value = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(root);
  return r;
}

static int object_get_required_boolean(mailjmap_json_value * object,
    const char * key, int * result)
{
  mailjmap_json_value * value;
  int r;

  if ((object == NULL) || (key == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  value = NULL;
  r = mailjmap_json_object_get(object, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_ERROR_PROTOCOL;
  r = mailjmap_json_boolean_value(value, result);
  mailjmap_json_free(value);
  return r;
}

static int object_get_required_int(mailjmap_json_value * object,
    const char * key, int * result)
{
  mailjmap_json_value * value;
  int64_t integer;
  int r;

  if ((object == NULL) || (key == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  value = NULL;
  integer = 0;
  r = mailjmap_json_object_get(object, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_ERROR_PROTOCOL;
  r = mailjmap_json_integer_value(value, &integer);
  mailjmap_json_free(value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (integer > 2147483647 || integer < -2147483647 - 1)
    return MAILJMAP_ERROR_PROTOCOL;

  * result = (int) integer;
  return MAILJMAP_NO_ERROR;
}

static int object_get_optional_int(mailjmap_json_value * object,
    const char * key, int * has_value, int * result)
{
  mailjmap_json_value * value;
  int64_t integer;
  int r;

  if ((object == NULL) || (key == NULL) || (has_value == NULL) ||
      (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * has_value = 0;
  value = NULL;
  integer = 0;
  r = mailjmap_json_object_get(object, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;

  r = mailjmap_json_integer_value(value, &integer);
  mailjmap_json_free(value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (integer > 2147483647 || integer < -2147483647 - 1)
    return MAILJMAP_ERROR_PROTOCOL;

  * has_value = 1;
  * result = (int) integer;
  return MAILJMAP_NO_ERROR;
}

static int object_get_nullable_string_dup(mailjmap_json_value * object,
    const char * key, char ** result)
{
  mailjmap_json_value * value;
  int r;

  if ((object == NULL) || (key == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  value = NULL;
  r = mailjmap_json_object_get(object, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(value)) {
    mailjmap_json_free(value);
    return MAILJMAP_NO_ERROR;
  }

  r = mailjmap_json_string_dup(value, result);
  mailjmap_json_free(value);
  return r;
}

static int parse_bool_map(mailjmap_json_value * object,
    const char * key, chash * result);

static int parse_mailbox(mailjmap_json_value * value,
    struct mailjmap_mailbox ** result)
{
  struct mailjmap_mailbox * mailbox;
  int r;

  if ((value == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  mailbox = mailjmap_mailbox_new();
  if (mailbox == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(value, "id", &mailbox->id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(value, "name", &mailbox->name);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "parentId",
      &mailbox->parent_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "role", &mailbox->role);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_required_int(value, "sortOrder", &mailbox->sort_order);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_required_boolean(value, "isSubscribed",
      &mailbox->is_subscribed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_required_int(value, "totalEmails", &mailbox->total_emails);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_required_int(value, "unreadEmails",
      &mailbox->unread_emails);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_required_int(value, "totalThreads",
      &mailbox->total_threads);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_required_int(value, "unreadThreads",
      &mailbox->unread_threads);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_bool_map(value, "myRights", mailbox->my_rights);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  if ((mailbox->id == NULL) || (mailbox->name == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  * result = mailbox;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_mailbox_free(mailbox);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_mailbox_list(mailjmap_json_value * arguments,
    struct mailjmap_mailbox_get_result * result)
{
  mailjmap_json_value * list;
  size_t count;
  size_t i;
  int r;

  list = NULL;
  r = mailjmap_json_object_get(arguments, "list", &list);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if ((list == NULL) || !mailjmap_json_is_array(list)) {
    mailjmap_json_free(list);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(list);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * value;
    struct mailjmap_mailbox * mailbox;

    value = NULL;
    mailbox = NULL;
    r = mailjmap_json_array_get(list, i, &value);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_mailbox(value, &mailbox);
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;

    if (clist_append(result->list, mailbox) < 0) {
      mailjmap_mailbox_free(mailbox);
      r = MAILJMAP_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjmap_json_free(list);
  return r;
}

static int parse_string_array(mailjmap_json_value * arguments,
    const char * key, clist * result)
{
  mailjmap_json_value * array;
  size_t count;
  size_t i;
  int r;

  array = NULL;
  r = mailjmap_json_object_get(arguments, key, &array);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (array == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_array(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(array);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * value;
    char * string;

    value = NULL;
    string = NULL;
    r = mailjmap_json_array_get(array, i, &value);
    if (r == MAILJMAP_NO_ERROR) {
      if (!mailjmap_json_is_string(value))
        r = MAILJMAP_ERROR_PROTOCOL;
      else
        r = mailjmap_json_string_dup(value, &string);
    }
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;

    if (clist_append(result, string) < 0) {
      free(string);
      r = MAILJMAP_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjmap_json_free(array);
  return r;
}

struct parse_string_object_keys_context {
  clist * list;
};

static int parse_string_object_key(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct parse_string_object_keys_context * keys_context;
  char * key_copy;

  (void) value;
  if ((key == NULL) || (context == NULL))
    return MAILJMAP_ERROR_PROTOCOL;

  key_copy = strdup(key);
  if (key_copy == NULL)
    return MAILJMAP_ERROR_MEMORY;

  keys_context = context;
  if (clist_append(keys_context->list, key_copy) < 0) {
    free(key_copy);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

static int parse_string_array_or_object_keys(mailjmap_json_value * arguments,
    const char * key, clist * result)
{
  struct parse_string_object_keys_context context;
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = mailjmap_json_object_get(arguments, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(value)) {
    mailjmap_json_free(value);
    return MAILJMAP_NO_ERROR;
  }
  if (mailjmap_json_is_array(value)) {
    mailjmap_json_free(value);
    return parse_string_array(arguments, key, result);
  }
  if (!mailjmap_json_is_object(value)) {
    mailjmap_json_free(value);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.list = result;
  r = mailjmap_json_object_foreach(value, parse_string_object_key,
      &context);
  mailjmap_json_free(value);
  return r;
}

static int parse_required_string_array(mailjmap_json_value * arguments,
    const char * key, clist * result)
{
  mailjmap_json_value * array;
  int r;

  array = NULL;
  r = mailjmap_json_object_get(arguments, key, &array);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (array == NULL)
    return MAILJMAP_ERROR_PROTOCOL;
  if (mailjmap_json_is_null(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_ERROR_PROTOCOL;
  }
  mailjmap_json_free(array);

  return parse_string_array(arguments, key, result);
}

static int parse_mailbox_get_arguments(mailjmap_json_value * arguments,
    struct mailjmap_mailbox_get_result ** result)
{
  struct mailjmap_mailbox_get_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_mailbox_get_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "state", &parsed->state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((parsed->account_id == NULL) || (parsed->state == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = parse_mailbox_list(arguments, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(arguments, "notFound", parsed->not_found);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_mailbox_get_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_thread(mailjmap_json_value * value,
    struct mailjmap_thread ** result)
{
  struct mailjmap_thread * thread;
  int r;

  if ((value == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  thread = mailjmap_thread_new();
  if (thread == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(value, "id", &thread->id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (thread->id == NULL) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = parse_required_string_array(value, "emailIds", thread->email_ids);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = thread;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_thread_free(thread);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_thread_list(mailjmap_json_value * arguments,
    struct mailjmap_thread_get_result * result)
{
  mailjmap_json_value * list;
  size_t count;
  size_t i;
  int r;

  list = NULL;
  r = mailjmap_json_object_get(arguments, "list", &list);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if ((list == NULL) || !mailjmap_json_is_array(list)) {
    mailjmap_json_free(list);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(list);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * value;
    struct mailjmap_thread * thread;

    value = NULL;
    thread = NULL;
    r = mailjmap_json_array_get(list, i, &value);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_thread(value, &thread);
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;

    if (clist_append(result->list, thread) < 0) {
      mailjmap_thread_free(thread);
      r = MAILJMAP_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjmap_json_free(list);
  return r;
}

static int parse_thread_get_arguments(mailjmap_json_value * arguments,
    struct mailjmap_thread_get_result ** result)
{
  struct mailjmap_thread_get_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_thread_get_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "state", &parsed->state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((parsed->account_id == NULL) || (parsed->state == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = parse_thread_list(arguments, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(arguments, "notFound", parsed->not_found);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_thread_get_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

struct parse_bool_map_context {
  chash * map;
};

static int parse_bool_map_entry(const char * key, mailjmap_json_value * value,
    void * context)
{
  struct parse_bool_map_context * map_context;
  chashdatum hash_key;
  chashdatum hash_value;
  int boolean;
  int r;

  if ((key == NULL) || (value == NULL) || (context == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  r = mailjmap_json_boolean_value(value, &boolean);
  if (r != MAILJMAP_NO_ERROR)
    return MAILJMAP_ERROR_PROTOCOL;

  map_context = context;
  hash_key.data = (void *) key;
  hash_key.len = (unsigned int) strlen(key) + 1;
  hash_value.data = &boolean;
  hash_value.len = sizeof(boolean);
  if (chash_set(map_context->map, &hash_key, &hash_value, NULL) < 0)
    return MAILJMAP_ERROR_MEMORY;

  return MAILJMAP_NO_ERROR;
}

static int parse_bool_map(mailjmap_json_value * object,
    const char * key, chash * result)
{
  struct parse_bool_map_context context;
  mailjmap_json_value * value;
  int r;

  if ((object == NULL) || (key == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  value = NULL;
  r = mailjmap_json_object_get(object, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;
  if (!mailjmap_json_is_object(value)) {
    mailjmap_json_free(value);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.map = result;
  r = mailjmap_json_object_foreach(value, parse_bool_map_entry, &context);
  mailjmap_json_free(value);
  return r;
}

struct parse_string_map_context {
  chash * map;
};

static int parse_string_map_entry(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct parse_string_map_context * map_context;
  chashdatum hash_key;
  chashdatum hash_value;
  char * string;
  int r;

  if ((key == NULL) || (value == NULL) || (context == NULL))
    return MAILJMAP_ERROR_BAD_STATE;
  if (!mailjmap_json_is_string(value))
    return MAILJMAP_ERROR_PROTOCOL;

  string = NULL;
  r = mailjmap_json_string_dup(value, &string);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  map_context = context;
  hash_key.data = (void *) key;
  hash_key.len = (unsigned int) strlen(key) + 1;
  hash_value.data = string;
  hash_value.len = (unsigned int) strlen(string) + 1;
  if (chash_set(map_context->map, &hash_key, &hash_value, NULL) < 0) {
    free(string);
    return MAILJMAP_ERROR_MEMORY;
  }

  free(string);
  return MAILJMAP_NO_ERROR;
}

static int parse_string_map(mailjmap_json_value * object,
    const char * key, chash * result)
{
  struct parse_string_map_context context;
  mailjmap_json_value * value;
  int r;

  if ((object == NULL) || (key == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  value = NULL;
  r = mailjmap_json_object_get(object, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(value)) {
    mailjmap_json_free(value);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_object(value)) {
    mailjmap_json_free(value);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.map = result;
  r = mailjmap_json_object_foreach(value, parse_string_map_entry, &context);
  mailjmap_json_free(value);
  return r;
}

static int duplicate_first_string_list_value(clist * list, char ** result)
{
  const char * value;

  if ((list == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  if (clist_begin(list) == NULL)
    return MAILJMAP_NO_ERROR;

  value = clist_content(clist_begin(list));
  if (value == NULL)
    return MAILJMAP_NO_ERROR;

  * result = strdup(value);
  if (* result == NULL)
    return MAILJMAP_ERROR_MEMORY;

  return MAILJMAP_NO_ERROR;
}

static int parse_first_emailer_array_email(mailjmap_json_value * object,
    const char * key, char ** result)
{
  mailjmap_json_value * array;
  mailjmap_json_value * value;
  int r;

  if ((object == NULL) || (key == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  array = NULL;
  value = NULL;
  r = mailjmap_json_object_get(object, key, &array);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (array == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_array(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_ERROR_PROTOCOL;
  }
  if (mailjmap_json_array_size(array) == 0) {
    mailjmap_json_free(array);
    return MAILJMAP_NO_ERROR;
  }

  r = mailjmap_json_array_get(array, 0, &value);
  if (r == MAILJMAP_NO_ERROR) {
    if (!mailjmap_json_is_object(value))
      r = MAILJMAP_ERROR_PROTOCOL;
    else
      r = mailjmap_json_object_get_string_dup(value, "email", result);
  }

  mailjmap_json_free(value);
  mailjmap_json_free(array);
  return r;
}

static int parse_email_header(mailjmap_json_value * value,
    struct mailjmap_email_header ** result)
{
  struct mailjmap_email_header * header;
  int r;

  if ((value == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  header = mailjmap_email_header_new();
  if (header == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(value, "name", &header->name);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(value, "value", &header->value);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((header->name == NULL) || (header->value == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  * result = header;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_email_header_free(header);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_email_header_list(mailjmap_json_value * object,
    const char * key, clist * result)
{
  mailjmap_json_value * array;
  size_t count;
  size_t i;
  int r;

  array = NULL;
  r = mailjmap_json_object_get(object, key, &array);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (array == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_array(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(array);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * value;
    struct mailjmap_email_header * header;

    value = NULL;
    header = NULL;
    r = mailjmap_json_array_get(array, i, &value);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_email_header(value, &header);
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;

    if (clist_append(result, header) < 0) {
      mailjmap_email_header_free(header);
      r = MAILJMAP_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjmap_json_free(array);
  return r;
}

static int parse_email_address(mailjmap_json_value * value,
    struct mailjmap_email_address ** result)
{
  struct mailjmap_email_address * address;
  int r;

  if ((value == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  address = mailjmap_email_address_new();
  if (address == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = object_get_nullable_string_dup(value, "name", &address->name);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "email", &address->email);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = address;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_email_address_free(address);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_email_address_list(mailjmap_json_value * object,
    const char * key, clist * result)
{
  mailjmap_json_value * array;
  size_t count;
  size_t i;
  int r;

  array = NULL;
  r = mailjmap_json_object_get(object, key, &array);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (array == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_array(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(array);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * value;
    struct mailjmap_email_address * address;

    value = NULL;
    address = NULL;
    r = mailjmap_json_array_get(array, i, &value);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_email_address(value, &address);
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;

    if (clist_append(result, address) < 0) {
      mailjmap_email_address_free(address);
      r = MAILJMAP_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjmap_json_free(array);
  return r;
}

static int parse_email_body_part(mailjmap_json_value * value,
    struct mailjmap_email_body_part ** result);

static int parse_body_part_language(mailjmap_json_value * object,
    struct mailjmap_email_body_part * part)
{
  mailjmap_json_value * value;
  size_t count;
  size_t i;
  int r;

  value = NULL;
  r = mailjmap_json_object_get(object, "language", &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(value)) {
    mailjmap_json_free(value);
    return MAILJMAP_NO_ERROR;
  }

  if (mailjmap_json_is_string(value)) {
    char * language;

    language = NULL;
    r = mailjmap_json_string_dup(value, &language);
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      return r;
    if (clist_append(part->languages, language) < 0) {
      free(language);
      return MAILJMAP_ERROR_MEMORY;
    }
    if (part->language == NULL) {
      part->language = strdup(language);
      if (part->language == NULL)
        return MAILJMAP_ERROR_MEMORY;
    }
    return MAILJMAP_NO_ERROR;
  }

  if (!mailjmap_json_is_array(value)) {
    mailjmap_json_free(value);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(value);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * item;
    char * language;

    item = NULL;
    language = NULL;
    r = mailjmap_json_array_get(value, i, &item);
    if (r == MAILJMAP_NO_ERROR) {
      if (!mailjmap_json_is_string(item))
        r = MAILJMAP_ERROR_PROTOCOL;
      else
        r = mailjmap_json_string_dup(item, &language);
    }
    mailjmap_json_free(item);
    if (r != MAILJMAP_NO_ERROR) {
      free(language);
      mailjmap_json_free(value);
      return r;
    }

    if (clist_append(part->languages, language) < 0) {
      free(language);
      mailjmap_json_free(value);
      return MAILJMAP_ERROR_MEMORY;
    }
    if (part->language == NULL) {
      part->language = strdup(language);
      if (part->language == NULL) {
        mailjmap_json_free(value);
        return MAILJMAP_ERROR_MEMORY;
      }
    }
  }

  mailjmap_json_free(value);
  return MAILJMAP_NO_ERROR;
}

static int parse_email_body_part_array(mailjmap_json_value * object,
    const char * key, clist * result)
{
  mailjmap_json_value * array;
  size_t count;
  size_t i;
  int r;

  array = NULL;
  r = mailjmap_json_object_get(object, key, &array);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (array == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_array(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(array);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * value;
    struct mailjmap_email_body_part * part;

    value = NULL;
    part = NULL;
    r = mailjmap_json_array_get(array, i, &value);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_email_body_part(value, &part);
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;

    if (clist_append(result, part) < 0) {
      mailjmap_email_body_part_free(part);
      r = MAILJMAP_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjmap_json_free(array);
  return r;
}

static int parse_email_body_part(mailjmap_json_value * value,
    struct mailjmap_email_body_part ** result)
{
  struct mailjmap_email_body_part * part;
  int r;

  if ((value == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  part = mailjmap_email_body_part_new();
  if (part == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = object_get_nullable_string_dup(value, "partId", &part->part_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "blobId", &part->blob_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "type", &part->type);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "name", &part->name);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "charset", &part->charset);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "disposition",
      &part->disposition);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "cid", &part->cid);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_body_part_language(value, part);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "location", &part->location);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_optional_int(value, "size", &part->has_size, &part->size);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_header_list(value, "headers", part->headers);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_body_part_array(value, "subParts", part->sub_parts);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = part;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_email_body_part_free(part);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

struct parse_body_values_context {
  clist * list;
};

static int parse_body_value_entry(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct parse_body_values_context * values_context;
  struct mailjmap_email_body_value * body_value;
  int r;

  if ((key == NULL) || (value == NULL) || (context == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  body_value = mailjmap_email_body_value_new();
  if (body_value == NULL)
    return MAILJMAP_ERROR_MEMORY;

  body_value->part_id = strdup(key);
  if (body_value->part_id == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }
  r = mailjmap_json_object_get_string_dup(value, "value",
      &body_value->value);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (body_value->value == NULL) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }
  r = object_get_required_boolean(value, "isTruncated",
      &body_value->is_truncated);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  values_context = context;
  if (clist_append(values_context->list, body_value) < 0) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_email_body_value_free(body_value);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_body_values(mailjmap_json_value * object, clist * result)
{
  struct parse_body_values_context context;
  mailjmap_json_value * values;
  int r;

  values = NULL;
  r = mailjmap_json_object_get(object, "bodyValues", &values);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (values == NULL)
    return MAILJMAP_NO_ERROR;
  if (!mailjmap_json_is_object(values)) {
    mailjmap_json_free(values);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.list = result;
  r = mailjmap_json_object_foreach(values, parse_body_value_entry, &context);
  mailjmap_json_free(values);
  return r;
}

static int parse_email_with_options(mailjmap_json_value * value,
    int require_id, struct mailjmap_email ** result)
{
  struct mailjmap_email * email;
  mailjmap_json_value * body_structure;
  int has_size;
  int r;

  if ((value == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  email = mailjmap_email_new();
  if (email == NULL)
    return MAILJMAP_ERROR_MEMORY;

  body_structure = NULL;
  r = mailjmap_json_object_get_string_dup(value, "id", &email->id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (require_id && (email->id == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = object_get_nullable_string_dup(value, "blobId", &email->blob_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "threadId", &email->thread_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_bool_map(value, "mailboxIds", email->mailbox_ids);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_bool_map(value, "keywords", email->keywords);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_optional_int(value, "size", &has_size, &email->size);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "receivedAt",
      &email->received_at);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(value, "messageId", email->message_ids);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = duplicate_first_string_list_value(email->message_ids,
      &email->message_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(value, "inReplyTo", email->in_reply_to_list);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = duplicate_first_string_list_value(email->in_reply_to_list,
      &email->in_reply_to);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(value, "references", email->references);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_address_list(value, "sender", email->sender);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_address_list(value, "from", email->from);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_address_list(value, "to", email->to);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_address_list(value, "cc", email->cc);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_address_list(value, "bcc", email->bcc);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_address_list(value, "replyTo", email->reply_to);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "subject", &email->subject);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "sentAt", &email->sent_at);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "preview", &email->preview);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_header_list(value, "headers", email->headers);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = mailjmap_json_object_get(value, "bodyStructure", &body_structure);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (body_structure != NULL) {
    r = parse_email_body_part(body_structure, &email->body_structure);
    mailjmap_json_free(body_structure);
    body_structure = NULL;
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  r = parse_email_body_part_array(value, "textBody", email->text_body);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_body_part_array(value, "htmlBody", email->html_body);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_email_body_part_array(value, "attachments", email->attachments);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_body_values(value, email->body_values);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = email;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(body_structure);
  mailjmap_email_free(email);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_email(mailjmap_json_value * value,
    struct mailjmap_email ** result)
{
  return parse_email_with_options(value, 1, result);
}

static int parse_email_without_id(mailjmap_json_value * value,
    struct mailjmap_email ** result)
{
  return parse_email_with_options(value, 0, result);
}

static int parse_email_list(mailjmap_json_value * arguments,
    struct mailjmap_email_get_result * result)
{
  mailjmap_json_value * list;
  size_t count;
  size_t i;
  int r;

  list = NULL;
  r = mailjmap_json_object_get(arguments, "list", &list);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if ((list == NULL) || !mailjmap_json_is_array(list)) {
    mailjmap_json_free(list);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(list);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * value;
    struct mailjmap_email * email;

    value = NULL;
    email = NULL;
    r = mailjmap_json_array_get(list, i, &value);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_email(value, &email);
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;

    if (clist_append(result->list, email) < 0) {
      mailjmap_email_free(email);
      r = MAILJMAP_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjmap_json_free(list);
  return r;
}

static int parse_email_get_arguments(mailjmap_json_value * arguments,
    struct mailjmap_email_get_result ** result)
{
  struct mailjmap_email_get_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_email_get_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "state", &parsed->state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((parsed->account_id == NULL) || (parsed->state == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = parse_email_list(arguments, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(arguments, "notFound", parsed->not_found);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_email_get_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_identity(mailjmap_json_value * value,
    struct mailjmap_identity ** result)
{
  struct mailjmap_identity * identity;
  int r;

  if ((value == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  identity = mailjmap_identity_new();
  if (identity == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(value, "id", &identity->id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(value, "name",
      &identity->name);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(value, "email",
      &identity->email);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((identity->id == NULL) || (identity->name == NULL) ||
      (identity->email == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = parse_first_emailer_array_email(value, "replyTo",
      &identity->reply_to);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_first_emailer_array_email(value, "bcc", &identity->bcc);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "textSignature",
      &identity->text_signature);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "htmlSignature",
      &identity->html_signature);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = identity;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_identity_free(identity);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_identity_list(mailjmap_json_value * arguments,
    struct mailjmap_identity_get_result * result)
{
  mailjmap_json_value * list;
  size_t count;
  size_t i;
  int r;

  list = NULL;
  r = mailjmap_json_object_get(arguments, "list", &list);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if ((list == NULL) || !mailjmap_json_is_array(list)) {
    mailjmap_json_free(list);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(list);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * value;
    struct mailjmap_identity * identity;

    value = NULL;
    identity = NULL;
    r = mailjmap_json_array_get(list, i, &value);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_identity(value, &identity);
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;

    if (clist_append(result->list, identity) < 0) {
      mailjmap_identity_free(identity);
      r = MAILJMAP_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjmap_json_free(list);
  return r;
}

static int parse_identity_get_arguments(mailjmap_json_value * arguments,
    struct mailjmap_identity_get_result ** result)
{
  struct mailjmap_identity_get_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_identity_get_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "state",
      &parsed->state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((parsed->account_id == NULL) || (parsed->state == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = parse_identity_list(arguments, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(arguments, "notFound", parsed->not_found);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_identity_get_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

struct parse_import_created_context {
  clist * list;
};

static int parse_import_created_entry(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct parse_import_created_context * created_context;
  struct mailjmap_import_created * created;
  int r;

  if ((key == NULL) || (value == NULL) || (context == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  created = mailjmap_import_created_new();
  if (created == NULL)
    return MAILJMAP_ERROR_MEMORY;

  created->creation_id = strdup(key);
  if (created->creation_id == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }
  r = parse_email(value, &created->email);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  created_context = context;
  if (clist_append(created_context->list, created) < 0) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_import_created_free(created);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_import_created_map(mailjmap_json_value * arguments,
    struct mailjmap_import_result * result)
{
  struct parse_import_created_context context;
  mailjmap_json_value * created;
  int r;

  created = NULL;
  r = mailjmap_json_object_get(arguments, "created", &created);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (created == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(created)) {
    mailjmap_json_free(created);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_object(created)) {
    mailjmap_json_free(created);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.list = result->created;
  r = mailjmap_json_object_foreach(created, parse_import_created_entry,
      &context);
  mailjmap_json_free(created);
  return r;
}

struct parse_import_not_created_context {
  clist * list;
};

static int parse_import_not_created_entry(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct parse_import_not_created_context * not_created_context;
  struct mailjmap_import_not_created * not_created;
  int r;

  if ((key == NULL) || (value == NULL) || (context == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  not_created = mailjmap_import_not_created_new();
  if (not_created == NULL)
    return MAILJMAP_ERROR_MEMORY;

  not_created->creation_id = strdup(key);
  if (not_created->creation_id == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }
  r = mailjmap_json_object_get_string_dup(value, "type",
      &not_created->type);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (not_created->type == NULL) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }
  r = object_get_nullable_string_dup(value, "description",
      &not_created->description);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  not_created_context = context;
  if (clist_append(not_created_context->list, not_created) < 0) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_import_not_created_free(not_created);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_import_not_created_map(mailjmap_json_value * arguments,
    struct mailjmap_import_result * result)
{
  struct parse_import_not_created_context context;
  mailjmap_json_value * not_created;
  int r;

  not_created = NULL;
  r = mailjmap_json_object_get(arguments, "notCreated", &not_created);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (not_created == NULL)
    return MAILJMAP_NO_ERROR;
  if (!mailjmap_json_is_object(not_created)) {
    mailjmap_json_free(not_created);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.list = result->not_created;
  r = mailjmap_json_object_foreach(not_created,
      parse_import_not_created_entry, &context);
  mailjmap_json_free(not_created);
  return r;
}

static int parse_email_import_arguments(mailjmap_json_value * arguments,
    struct mailjmap_import_result ** result)
{
  struct mailjmap_import_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_import_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "oldState",
      &parsed->old_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "newState",
      &parsed->new_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((parsed->account_id == NULL) || (parsed->old_state == NULL) ||
      (parsed->new_state == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = parse_import_created_map(arguments, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_import_not_created_map(arguments, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_import_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

struct parse_email_parse_context {
  struct mailjmap_email_parse_result * result;
};

static int parse_email_parse_entry(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct parse_email_parse_context * parse_context;
  struct mailjmap_email_parse_item * item;
  char * not_parsable;
  int r;

  if ((key == NULL) || (value == NULL) || (context == NULL))
    return MAILJMAP_ERROR_PROTOCOL;

  parse_context = context;
  if (mailjmap_json_is_null(value)) {
    not_parsable = strdup(key);
    if (not_parsable == NULL)
      return MAILJMAP_ERROR_MEMORY;
    if (clist_append(parse_context->result->not_parsable, not_parsable) < 0) {
      free(not_parsable);
      return MAILJMAP_ERROR_MEMORY;
    }
    return MAILJMAP_NO_ERROR;
  }

  if (!mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  item = mailjmap_email_parse_item_new();
  if (item == NULL)
    return MAILJMAP_ERROR_MEMORY;

  item->blob_id = strdup(key);
  if (item->blob_id == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }
  r = parse_email_without_id(value, &item->email);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  if (clist_append(parse_context->result->parsed, item) < 0) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_email_parse_item_free(item);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_email_parse_map(mailjmap_json_value * arguments,
    struct mailjmap_email_parse_result * result)
{
  struct parse_email_parse_context context;
  mailjmap_json_value * parsed;
  int r;

  parsed = NULL;
  r = mailjmap_json_object_get(arguments, "parsed", &parsed);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if ((parsed == NULL) || !mailjmap_json_is_object(parsed)) {
    mailjmap_json_free(parsed);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.result = result;
  r = mailjmap_json_object_foreach(parsed, parse_email_parse_entry,
      &context);
  mailjmap_json_free(parsed);
  return r;
}

static int parse_email_parse_arguments(mailjmap_json_value * arguments,
    struct mailjmap_email_parse_result ** result)
{
  struct mailjmap_email_parse_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_email_parse_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (parsed->account_id == NULL) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = parse_email_parse_map(arguments, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(arguments, "notParsable", parsed->not_parsable);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_email_parse_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_search_snippet(mailjmap_json_value * value,
    struct mailjmap_search_snippet ** result)
{
  struct mailjmap_search_snippet * snippet;
  int r;

  if ((value == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  snippet = mailjmap_search_snippet_new();
  if (snippet == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(value, "emailId",
      &snippet->email_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (snippet->email_id == NULL) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = object_get_nullable_string_dup(value, "subject", &snippet->subject);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "preview", &snippet->preview);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = snippet;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_search_snippet_free(snippet);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_search_snippet_list(mailjmap_json_value * arguments,
    struct mailjmap_search_snippet_get_result * result)
{
  mailjmap_json_value * list;
  size_t count;
  size_t i;
  int r;

  list = NULL;
  r = mailjmap_json_object_get(arguments, "list", &list);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if ((list == NULL) || !mailjmap_json_is_array(list)) {
    mailjmap_json_free(list);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(list);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * value;
    struct mailjmap_search_snippet * snippet;

    value = NULL;
    snippet = NULL;
    r = mailjmap_json_array_get(list, i, &value);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_search_snippet(value, &snippet);
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;

    if (clist_append(result->list, snippet) < 0) {
      mailjmap_search_snippet_free(snippet);
      r = MAILJMAP_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjmap_json_free(list);
  return r;
}

static int parse_search_snippet_get_arguments(
    mailjmap_json_value * arguments,
    struct mailjmap_search_snippet_get_result ** result)
{
  struct mailjmap_search_snippet_get_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_search_snippet_get_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (parsed->account_id == NULL) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = parse_search_snippet_list(arguments, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(arguments, "notFound", parsed->not_found);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_search_snippet_get_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

struct parse_set_created_context {
  clist * list;
};

static int parse_set_created_envelope(mailjmap_json_value * object,
    struct mailjmap_set_created * created)
{
  mailjmap_json_value * envelope;
  mailjmap_json_value * mail_from;
  int r;

  envelope = NULL;
  mail_from = NULL;
  r = mailjmap_json_object_get(object, "envelope", &envelope);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (envelope == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(envelope)) {
    mailjmap_json_free(envelope);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_object(envelope)) {
    mailjmap_json_free(envelope);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  r = mailjmap_json_object_get(envelope, "mailFrom", &mail_from);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  if (mail_from != NULL) {
    if (mailjmap_json_is_null(mail_from)) {
      mailjmap_json_free(mail_from);
      mail_from = NULL;
    }
    else {
      r = parse_email_address(mail_from, &created->envelope_mail_from);
      mailjmap_json_free(mail_from);
      mail_from = NULL;
      if (r != MAILJMAP_NO_ERROR)
        goto cleanup;
    }
  }

  r = parse_email_address_list(envelope, "rcptTo",
      created->envelope_rcpt_to);

 cleanup:
  mailjmap_json_free(mail_from);
  mailjmap_json_free(envelope);
  return r;
}

struct parse_delivery_status_context {
  clist * list;
};

static int parse_delivery_status_entry(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct parse_delivery_status_context * status_context;
  struct mailjmap_email_submission_delivery_status * status;
  int r;

  if ((key == NULL) || (value == NULL) || (context == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  status = mailjmap_email_submission_delivery_status_new();
  if (status == NULL)
    return MAILJMAP_ERROR_MEMORY;

  status->email = strdup(key);
  if (status->email == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  r = object_get_nullable_string_dup(value, "smtpReply",
      &status->smtp_reply);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "delivered",
      &status->delivered);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(value, "displayed",
      &status->displayed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  status_context = context;
  if (clist_append(status_context->list, status) < 0) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_email_submission_delivery_status_free(status);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_delivery_status_map(mailjmap_json_value * object,
    struct mailjmap_set_created * created)
{
  struct parse_delivery_status_context context;
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = mailjmap_json_object_get(object, "deliveryStatus", &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(value)) {
    mailjmap_json_free(value);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_object(value)) {
    mailjmap_json_free(value);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.list = created->delivery_status;
  r = mailjmap_json_object_foreach(value, parse_delivery_status_entry,
      &context);
  mailjmap_json_free(value);
  return r;
}

static int parse_set_created_submission_fields(mailjmap_json_value * value,
    struct mailjmap_set_created * created)
{
  int r;

  r = object_get_nullable_string_dup(value, "emailId", &created->email_id);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = object_get_nullable_string_dup(value, "identityId",
      &created->identity_id);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = object_get_nullable_string_dup(value, "threadId",
      &created->thread_id);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = object_get_nullable_string_dup(value, "sendAt", &created->send_at);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = object_get_nullable_string_dup(value, "undoStatus",
      &created->undo_status);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = parse_set_created_envelope(value, created);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = parse_delivery_status_map(value, created);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  return parse_string_map(value, "dsnBlobIds", created->dsn_blob_ids);
}

static int parse_set_created_entry(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct parse_set_created_context * created_context;
  struct mailjmap_set_created * created;
  int r;

  if ((key == NULL) || (value == NULL) || (context == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  created = mailjmap_set_created_new();
  if (created == NULL)
    return MAILJMAP_ERROR_MEMORY;

  created->creation_id = strdup(key);
  if (created->creation_id == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }
  r = mailjmap_json_object_get_string_dup(value, "id", &created->id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (created->id == NULL) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }
  r = parse_set_created_submission_fields(value, created);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  created_context = context;
  if (clist_append(created_context->list, created) < 0) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_set_created_free(created);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_set_created_map(mailjmap_json_value * arguments,
    struct mailjmap_set_result * result)
{
  struct parse_set_created_context context;
  mailjmap_json_value * created;
  int r;

  created = NULL;
  r = mailjmap_json_object_get(arguments, "created", &created);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (created == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(created)) {
    mailjmap_json_free(created);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_object(created)) {
    mailjmap_json_free(created);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.list = result->created;
  r = mailjmap_json_object_foreach(created, parse_set_created_entry,
      &context);
  mailjmap_json_free(created);
  return r;
}

struct parse_set_error_context {
  clist * list;
};

static int parse_set_error_entry(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct parse_set_error_context * error_context;
  struct mailjmap_set_error * error;
  int r;

  if ((key == NULL) || (value == NULL) || (context == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  error = mailjmap_set_error_new();
  if (error == NULL)
    return MAILJMAP_ERROR_MEMORY;

  error->id = strdup(key);
  if (error->id == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }
  r = mailjmap_json_object_get_string_dup(value, "type", &error->type);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (error->type == NULL) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }
  r = object_get_nullable_string_dup(value, "description",
      &error->description);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  error_context = context;
  if (clist_append(error_context->list, error) < 0) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_set_error_free(error);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_set_error_map(mailjmap_json_value * arguments,
    const char * key, clist * result)
{
  struct parse_set_error_context context;
  mailjmap_json_value * errors;
  int r;

  errors = NULL;
  r = mailjmap_json_object_get(arguments, key, &errors);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (errors == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(errors)) {
    mailjmap_json_free(errors);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_object(errors)) {
    mailjmap_json_free(errors);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.list = result;
  r = mailjmap_json_object_foreach(errors, parse_set_error_entry,
      &context);
  mailjmap_json_free(errors);
  return r;
}

static int parse_set_arguments(mailjmap_json_value * arguments,
    struct mailjmap_set_result ** result)
{
  struct mailjmap_set_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_set_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (parsed->account_id == NULL) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }
  r = object_get_nullable_string_dup(arguments, "oldState",
      &parsed->old_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_nullable_string_dup(arguments, "newState",
      &parsed->new_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = parse_set_created_map(arguments, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array_or_object_keys(arguments, "updated",
      parsed->updated);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(arguments, "destroyed", parsed->destroyed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_set_error_map(arguments, "notCreated", parsed->not_created);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_set_error_map(arguments, "notUpdated", parsed->not_updated);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_set_error_map(arguments, "notDestroyed",
      parsed->not_destroyed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_set_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_changes_arguments(mailjmap_json_value * arguments,
    struct mailjmap_changes_result ** result)
{
  struct mailjmap_changes_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_changes_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "oldState",
      &parsed->old_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "newState",
      &parsed->new_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((parsed->account_id == NULL) || (parsed->old_state == NULL) ||
      (parsed->new_state == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = object_get_required_boolean(arguments, "hasMoreChanges",
      &parsed->has_more_changes);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_required_string_array(arguments, "created", parsed->created);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_required_string_array(arguments, "updated", parsed->updated);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_required_string_array(arguments, "destroyed", parsed->destroyed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_changes_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_query_arguments(mailjmap_json_value * arguments,
    struct mailjmap_query_result ** result)
{
  struct mailjmap_query_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_query_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "queryState",
      &parsed->query_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((parsed->account_id == NULL) || (parsed->query_state == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = object_get_required_boolean(arguments, "canCalculateChanges",
      &parsed->can_calculate_changes);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_required_int(arguments, "position", &parsed->position);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_optional_int(arguments, "total", &parsed->has_total,
      &parsed->total);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_required_string_array(arguments, "ids", parsed->ids);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_query_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_query_change(mailjmap_json_value * value,
    struct mailjmap_query_change ** result)
{
  struct mailjmap_query_change * change;
  int r;

  if ((value == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  change = mailjmap_query_change_new();
  if (change == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(value, "id", &change->id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_get_required_int(value, "index", &change->index);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (change->id == NULL) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  * result = change;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_query_change_free(change);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static int parse_query_change_array(mailjmap_json_value * arguments,
    const char * key, clist * result)
{
  mailjmap_json_value * array;
  size_t count;
  size_t i;
  int r;

  array = NULL;
  r = mailjmap_json_object_get(arguments, key, &array);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (array == NULL)
    return MAILJMAP_NO_ERROR;
  if (mailjmap_json_is_null(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_NO_ERROR;
  }
  if (!mailjmap_json_is_array(array)) {
    mailjmap_json_free(array);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  count = mailjmap_json_array_size(array);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * value;
    struct mailjmap_query_change * change;

    value = NULL;
    change = NULL;
    r = mailjmap_json_array_get(array, i, &value);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_query_change(value, &change);
    mailjmap_json_free(value);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;

    if (clist_append(result, change) < 0) {
      mailjmap_query_change_free(change);
      r = MAILJMAP_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjmap_json_free(array);
  return r;
}

static int parse_query_changes_arguments(mailjmap_json_value * arguments,
    struct mailjmap_query_changes_result ** result)
{
  struct mailjmap_query_changes_result * parsed;
  int r;

  if ((arguments == NULL) || (result == NULL) ||
      !mailjmap_json_is_object(arguments))
    return MAILJMAP_ERROR_PROTOCOL;

  parsed = mailjmap_query_changes_result_new();
  if (parsed == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(arguments, "accountId",
      &parsed->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "oldQueryState",
      &parsed->old_query_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(arguments, "newQueryState",
      &parsed->new_query_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((parsed->account_id == NULL) || (parsed->old_query_state == NULL) ||
      (parsed->new_query_state == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = object_get_optional_int(arguments, "total", &parsed->has_total,
      &parsed->total);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_string_array(arguments, "removed", parsed->removed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_query_change_array(arguments, "added", parsed->added);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_query_changes_result_free(parsed);
  return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;
}

static struct mailjmap_method_response * find_method_response(
    struct mailjmap_response * response, const char * name)
{
  clistiter * cur;

  if ((response == NULL) || (name == NULL))
    return NULL;

  for (cur = clist_begin(response->method_responses); cur != NULL;
       cur = clist_next(cur)) {
    struct mailjmap_method_response * method_response;

    method_response = clist_content(cur);
    if ((method_response != NULL) &&
        (strcmp(method_response->name, name) == 0))
      return method_response;
  }

  return NULL;
}

static int method_error_type_is(const char * type, const char * expected)
{
  return (type != NULL) && (expected != NULL) &&
      (strcmp(type, expected) == 0);
}

static int method_error_response_code(
    struct mailjmap_method_response * method_response)
{
  char * type;
  int r;
  int code;

  if (method_response == NULL)
    return MAILJMAP_ERROR_PROTOCOL;

  type = NULL;
  r = mailjmap_json_object_get_string_dup(method_response->arguments, "type",
      &type);
  if (r != MAILJMAP_NO_ERROR)
    return (r == MAILJMAP_ERROR_BAD_STATE) ? MAILJMAP_ERROR_PROTOCOL : r;

  code = MAILJMAP_ERROR_METHOD;
  if (method_error_type_is(type, "unknownCapability") ||
      method_error_type_is(type, "accountNotSupportedByMethod"))
    code = MAILJMAP_ERROR_CAPABILITY;
  else if (method_error_type_is(type, "requestTooLarge") ||
      method_error_type_is(type, "tooManyChanges") ||
      method_error_type_is(type, "tooManyKeywords") ||
      method_error_type_is(type, "tooManyMailboxes") ||
      method_error_type_is(type, "tooLarge") ||
      method_error_type_is(type, "overQuota"))
    code = MAILJMAP_ERROR_LIMIT;
  else if (method_error_type_is(type, "invalidResultReference"))
    code = MAILJMAP_ERROR_PROTOCOL;

  free(type);
  return code;
}

int mailjmap_mailbox_get(mailjmap * session,
    const char * account_id,
    clist * ids,
    clist * properties,
    struct mailjmap_mailbox_get_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_mailbox_get_arguments(account_id, ids, properties, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Mailbox/get", arguments, "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Mailbox/get");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_mailbox_get_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_mailbox_changes(mailjmap * session,
    const char * account_id,
    const char * since_state,
    struct mailjmap_changes_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (since_state == NULL) ||
      (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_mailbox_changes_arguments(account_id, since_state, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Mailbox/changes", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Mailbox/changes");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_changes_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_mailbox_query(mailjmap * session,
    const char * account_id,
    int position,
    int limit,
    struct mailjmap_query_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_mailbox_query_arguments(account_id, position, limit, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Mailbox/query", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Mailbox/query");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_query_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_mailbox_set(mailjmap * session,
    const char * account_id,
    const char * if_in_state,
    clist * create,
    clist * update,
    clist * destroy,
    struct mailjmap_set_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_mailbox_set_arguments(account_id, if_in_state, create, update,
      destroy, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Mailbox/set", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Mailbox/set");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_set_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_thread_get(mailjmap * session,
    const char * account_id,
    clist * ids,
    struct mailjmap_thread_get_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_mailbox_get_arguments(account_id, ids, NULL, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Thread/get", arguments, "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Thread/get");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_thread_get_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_thread_changes(mailjmap * session,
    const char * account_id,
    const char * since_state,
    struct mailjmap_changes_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (since_state == NULL) ||
      (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_mailbox_changes_arguments(account_id, since_state, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Thread/changes", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Thread/changes");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_changes_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_email_query(mailjmap * session,
    const char * account_id,
    int position,
    int limit,
    struct mailjmap_query_result ** result)
{
  return mailjmap_email_query_with_text_filter(session, account_id, NULL,
      position, limit, result);
}

int mailjmap_email_query_with_text_filter(mailjmap * session,
    const char * account_id,
    const char * text,
    int position,
    int limit,
    struct mailjmap_query_result ** result)
{
  return mailjmap_email_query_with_options(session, account_id, text,
      position, NULL, 0, limit, -1, -1, result);
}

int mailjmap_email_query_with_options(mailjmap * session,
    const char * account_id,
    const char * text,
    int position,
    const char * anchor,
    int anchor_offset,
    int limit,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_result ** result)
{
  return mailjmap_email_query_with_sort_options(session, account_id, text,
      NULL, position, anchor, anchor_offset, limit, calculate_total,
      collapse_threads, result);
}

static int email_query_execute(mailjmap * session,
    const char * account_id,
    const char * text,
    struct mailjmap_email_query_filter * filter,
    clist * sort,
    int position,
    const char * anchor,
    int anchor_offset,
    int limit,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_email_query_arguments(account_id, text, filter, sort, position,
      anchor, anchor_offset, limit, calculate_total, collapse_threads,
      &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Email/query", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Email/query");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_query_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_email_query_with_sort_options(mailjmap * session,
    const char * account_id,
    const char * text,
    clist * sort,
    int position,
    const char * anchor,
    int anchor_offset,
    int limit,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_result ** result)
{
  return email_query_execute(session, account_id, text, NULL, sort, position,
      anchor, anchor_offset, limit, calculate_total, collapse_threads,
      result);
}

int mailjmap_email_query_with_filter_options(mailjmap * session,
    const char * account_id,
    struct mailjmap_email_query_filter * filter,
    clist * sort,
    int position,
    const char * anchor,
    int anchor_offset,
    int limit,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_result ** result)
{
  return email_query_execute(session, account_id, NULL, filter, sort,
      position, anchor, anchor_offset, limit, calculate_total,
      collapse_threads, result);
}

int mailjmap_email_query_changes(mailjmap * session,
    const char * account_id,
    const char * since_query_state,
    struct mailjmap_query_changes_result ** result)
{
  return mailjmap_email_query_changes_with_text_filter(session, account_id,
      since_query_state, NULL, result);
}

int mailjmap_email_query_changes_with_text_filter(mailjmap * session,
    const char * account_id,
    const char * since_query_state,
    const char * text,
    struct mailjmap_query_changes_result ** result)
{
  return mailjmap_email_query_changes_with_options(session, account_id,
      since_query_state, text, -1, NULL, -1, -1, result);
}

static int email_query_changes_execute(mailjmap * session,
    const char * account_id,
    const char * since_query_state,
    const char * text,
    struct mailjmap_email_query_filter * filter,
    clist * sort,
    int max_changes,
    const char * up_to_id,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_changes_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) ||
      (since_query_state == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_query_changes_arguments(account_id, since_query_state, text,
      filter, sort, max_changes, up_to_id, calculate_total,
      collapse_threads, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Email/queryChanges",
      arguments, "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Email/queryChanges");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_query_changes_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_email_query_changes_with_options(mailjmap * session,
    const char * account_id,
    const char * since_query_state,
    const char * text,
    int max_changes,
    const char * up_to_id,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_changes_result ** result)
{
  return email_query_changes_execute(session, account_id, since_query_state,
      text, NULL, NULL, max_changes, up_to_id, calculate_total,
      collapse_threads, result);
}

int mailjmap_email_query_changes_with_filter_options(mailjmap * session,
    const char * account_id,
    const char * since_query_state,
    struct mailjmap_email_query_filter * filter,
    clist * sort,
    int max_changes,
    const char * up_to_id,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_changes_result ** result)
{
  return email_query_changes_execute(session, account_id, since_query_state,
      NULL, filter, sort, max_changes, up_to_id, calculate_total,
      collapse_threads, result);
}

int mailjmap_email_changes(mailjmap * session,
    const char * account_id,
    const char * since_state,
    struct mailjmap_changes_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (since_state == NULL) ||
      (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_mailbox_changes_arguments(account_id, since_state, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Email/changes", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Email/changes");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_changes_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_email_get(mailjmap * session,
    const char * account_id,
    clist * ids,
    clist * properties,
    struct mailjmap_email_get_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_mailbox_get_arguments(account_id, ids, properties, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Email/get", arguments, "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Email/get");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_email_get_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_email_set(mailjmap * session,
    const char * account_id,
    const char * if_in_state,
    clist * create,
    clist * update,
    clist * destroy,
    struct mailjmap_set_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_email_set_arguments(account_id, if_in_state, create, update,
      destroy, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Email/set", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Email/set");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_set_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_email_copy(mailjmap * session,
    const char * account_id,
    const char * from_account_id,
    const char * if_from_in_state,
    clist * create,
    int on_success_destroy_original,
    struct mailjmap_set_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) ||
      (from_account_id == NULL) || (create == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_email_copy_arguments(account_id, from_account_id,
      if_from_in_state, create, on_success_destroy_original, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Email/copy", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Email/copy");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_set_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_email_import(mailjmap * session,
    const char * account_id,
    const char * creation_id,
    const char * blob_id,
    clist * mailbox_ids,
    clist * keywords,
    const char * received_at,
    struct mailjmap_import_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (creation_id == NULL) ||
      (blob_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_email_import_arguments(account_id, creation_id, blob_id,
      mailbox_ids, keywords, received_at, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Email/import", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Email/import");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_email_import_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_email_parse(mailjmap * session,
    const char * account_id,
    clist * blob_ids,
    clist * properties,
    clist * body_properties,
    struct mailjmap_email_parse_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (blob_ids == NULL) ||
      (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_email_parse_arguments(account_id, blob_ids, properties,
      body_properties, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Email/parse", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Email/parse");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_email_parse_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_search_snippet_get(mailjmap * session,
    const char * account_id,
    clist * email_ids,
    struct mailjmap_search_snippet_get_result ** result)
{
  return mailjmap_search_snippet_get_with_text_filter(session, account_id,
      email_ids, NULL, result);
}

static int search_snippet_get_execute(mailjmap * session,
    const char * account_id,
    clist * email_ids,
    const char * text,
    struct mailjmap_email_query_filter * filter,
    struct mailjmap_search_snippet_get_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (email_ids == NULL) ||
      (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_search_snippet_get_arguments(account_id, email_ids, text, filter,
      &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "SearchSnippet/get",
      arguments, "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "SearchSnippet/get");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_search_snippet_get_arguments(method_response->arguments,
      result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_search_snippet_get_with_text_filter(mailjmap * session,
    const char * account_id,
    clist * email_ids,
    const char * text,
    struct mailjmap_search_snippet_get_result ** result)
{
  return search_snippet_get_execute(session, account_id, email_ids, text,
      NULL, result);
}

int mailjmap_search_snippet_get_with_filter(mailjmap * session,
    const char * account_id,
    clist * email_ids,
    struct mailjmap_email_query_filter * filter,
    struct mailjmap_search_snippet_get_result ** result)
{
  return search_snippet_get_execute(session, account_id, email_ids, NULL,
      filter, result);
}

int mailjmap_identity_get(mailjmap * session,
    const char * account_id,
    clist * ids,
    clist * properties,
    struct mailjmap_identity_get_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_capability(request,
      MAILJMAP_CAPABILITY_SUBMISSION);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_mailbox_get_arguments(account_id, ids, properties, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Identity/get", arguments,
      "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "Identity/get");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_identity_get_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

int mailjmap_email_submission_set(mailjmap * session,
    const char * account_id,
    const char * if_in_state,
    clist * create,
    clist * update,
    clist * destroy,
    struct mailjmap_set_result ** result)
{
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  mailjmap_json_value * arguments;
  int r;

  if ((session == NULL) || (account_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_request_add_capability(request, MAILJMAP_CAPABILITY_MAIL);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_capability(request,
      MAILJMAP_CAPABILITY_SUBMISSION);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = build_email_submission_set_arguments(account_id, if_in_state, create,
      update, destroy, &arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "EmailSubmission/set",
      arguments, "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  arguments = NULL;

  r = mailjmap_call(session, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = find_method_response(response, "EmailSubmission/set");
  if (method_response == NULL) {
    method_response = find_method_response(response, "error");
    r = method_error_response_code(method_response);
    goto cleanup;
  }

  r = parse_set_arguments(method_response->arguments, result);

 cleanup:
  mailjmap_json_free(arguments);
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  return r;
}

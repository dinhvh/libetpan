/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILJMAP_MAIL_H

#define MAILJMAP_MAIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/clist.h>
#include <libetpan/chash.h>
#include <libetpan/mailjmap_types.h>

struct mailjmap_mailbox {
  char * id;
  char * name;
  char * parent_id;
  char * role;
  int sort_order;
  int is_subscribed;
  int total_emails;
  int unread_emails;
  int total_threads;
  int unread_threads;
  chash /* char * -> int boolean */ * my_rights;
};

struct mailjmap_thread {
  char * id;
  clist /* char * */ * email_ids;
};

struct mailjmap_email_header {
  char * name;
  char * value;
};

struct mailjmap_email_address {
  char * name;
  char * email;
};

struct mailjmap_email_body_part {
  char * part_id;
  char * blob_id;
  char * type;
  char * name;
  char * charset;
  char * disposition;
  char * cid;
  char * language;
  clist /* char * */ * languages;
  char * location;
  int size;
  int has_size;
  clist /* struct mailjmap_email_header * */ * headers;
  clist /* struct mailjmap_email_body_part * */ * sub_parts;
};

struct mailjmap_email_body_value {
  char * part_id;
  char * value;
  int is_truncated;
};

struct mailjmap_email {
  char * id;
  char * blob_id;
  char * thread_id;
  chash /* char * -> int boolean */ * mailbox_ids;
  chash /* char * -> int boolean */ * keywords;
  int size;
  char * received_at;
  char * message_id;
  char * in_reply_to;
  clist /* char * */ * message_ids;
  clist /* char * */ * in_reply_to_list;
  clist /* char * */ * references;
  clist /* struct mailjmap_email_address * */ * sender;
  clist /* struct mailjmap_email_address * */ * from;
  clist /* struct mailjmap_email_address * */ * to;
  clist /* struct mailjmap_email_address * */ * cc;
  clist /* struct mailjmap_email_address * */ * bcc;
  clist /* struct mailjmap_email_address * */ * reply_to;
  char * subject;
  char * sent_at;
  char * preview;
  clist /* struct mailjmap_email_header * */ * headers;
  struct mailjmap_email_body_part * body_structure;
  clist /* struct mailjmap_email_body_part * */ * text_body;
  clist /* struct mailjmap_email_body_part * */ * html_body;
  clist /* struct mailjmap_email_body_part * */ * attachments;
  clist /* struct mailjmap_email_body_value * */ * body_values;
};

struct mailjmap_identity {
  char * id;
  char * name;
  char * email;
  char * reply_to;
  char * bcc;
  char * text_signature;
  char * html_signature;
};

struct mailjmap_mailbox_get_result {
  char * account_id;
  char * state;
  clist /* struct mailjmap_mailbox * */ * list;
  clist /* char * */ * not_found;
};

struct mailjmap_thread_get_result {
  char * account_id;
  char * state;
  clist /* struct mailjmap_thread * */ * list;
  clist /* char * */ * not_found;
};

struct mailjmap_email_get_result {
  char * account_id;
  char * state;
  clist /* struct mailjmap_email * */ * list;
  clist /* char * */ * not_found;
};

struct mailjmap_identity_get_result {
  char * account_id;
  char * state;
  clist /* struct mailjmap_identity * */ * list;
  clist /* char * */ * not_found;
};

struct mailjmap_changes_result {
  char * account_id;
  char * old_state;
  char * new_state;
  int has_more_changes;
  clist /* char * */ * created;
  clist /* char * */ * updated;
  clist /* char * */ * destroyed;
};

struct mailjmap_query_result {
  char * account_id;
  char * query_state;
  int can_calculate_changes;
  int position;
  int total;
  int has_total;
  clist /* char * */ * ids;
};

struct mailjmap_query_change {
  char * id;
  int index;
};

struct mailjmap_query_changes_result {
  char * account_id;
  char * old_query_state;
  char * new_query_state;
  int total;
  int has_total;
  clist /* char * */ * removed;
  clist /* struct mailjmap_query_change * */ * added;
};

struct mailjmap_email_query_sort_comparator {
  char * property;
  int has_is_ascending;
  int is_ascending;
  char * collation;
};

struct mailjmap_email_query_filter {
  int is_operator;
  char * filter_operator;
  clist /* struct mailjmap_email_query_filter * */ * conditions;
  char * in_mailbox;
  char * before;
  char * after;
  int has_min_size;
  int min_size;
  int has_max_size;
  int max_size;
  char * has_keyword;
  char * not_keyword;
  char * text;
  char * from;
  char * to;
  char * cc;
  char * bcc;
  char * subject;
  char * body;
};

struct mailjmap_import_created {
  char * creation_id;
  struct mailjmap_email * email;
};

struct mailjmap_import_not_created {
  char * creation_id;
  char * type;
  char * description;
};

struct mailjmap_import_result {
  char * account_id;
  char * old_state;
  char * new_state;
  clist /* struct mailjmap_import_created * */ * created;
  clist /* struct mailjmap_import_not_created * */ * not_created;
};

struct mailjmap_email_parse_item {
  char * blob_id;
  struct mailjmap_email * email;
};

struct mailjmap_email_parse_result {
  char * account_id;
  clist /* struct mailjmap_email_parse_item * */ * parsed;
  clist /* char * */ * not_parsable;
};

struct mailjmap_search_snippet {
  char * email_id;
  char * subject;
  char * preview;
};

struct mailjmap_search_snippet_get_result {
  char * account_id;
  clist /* struct mailjmap_search_snippet * */ * list;
  clist /* char * */ * not_found;
};

struct mailjmap_mailbox_set_item {
  char * id;
  char * name;
  int has_parent_id;
  int parent_id_is_null;
  char * parent_id;
  int has_role;
  int role_is_null;
  char * role;
  int has_sort_order;
  int sort_order;
  int has_is_subscribed;
  int is_subscribed;
};

struct mailjmap_email_set_item {
  char * id;
  int has_mailbox_ids;
  clist /* char * */ * mailbox_ids;
  int has_keywords;
  clist /* char * */ * keywords;
};

struct mailjmap_email_copy_item {
  char * creation_id;
  char * id;
  int has_mailbox_ids;
  clist /* char * */ * mailbox_ids;
  int has_keywords;
  clist /* char * */ * keywords;
  char * received_at;
};

struct mailjmap_email_submission_set_item {
  char * id;
  char * email_id;
  char * identity_id;
  char * send_at;
  char * undo_status;
  struct mailjmap_email_address * envelope_mail_from;
  clist /* struct mailjmap_email_address * */ * envelope_rcpt_to;
};

struct mailjmap_email_submission_delivery_status {
  char * email;
  char * smtp_reply;
  char * delivered;
  char * displayed;
};

struct mailjmap_set_created {
  char * creation_id;
  char * id;
  char * email_id;
  char * identity_id;
  char * thread_id;
  char * send_at;
  char * undo_status;
  struct mailjmap_email_address * envelope_mail_from;
  clist /* struct mailjmap_email_address * */ * envelope_rcpt_to;
  clist /* struct mailjmap_email_submission_delivery_status * */ * delivery_status;
  chash /* char * -> char * */ * dsn_blob_ids;
};

struct mailjmap_set_error {
  char * id;
  char * type;
  char * description;
};

struct mailjmap_set_result {
  char * account_id;
  char * old_state;
  char * new_state;
  clist /* struct mailjmap_set_created * */ * created;
  clist /* char * */ * updated;
  clist /* char * */ * destroyed;
  clist /* struct mailjmap_set_error * */ * not_created;
  clist /* struct mailjmap_set_error * */ * not_updated;
  clist /* struct mailjmap_set_error * */ * not_destroyed;
};

LIBETPAN_EXPORT
struct mailjmap_mailbox * mailjmap_mailbox_new(void);
LIBETPAN_EXPORT
void mailjmap_mailbox_free(struct mailjmap_mailbox * mailbox);

LIBETPAN_EXPORT
struct mailjmap_thread * mailjmap_thread_new(void);
LIBETPAN_EXPORT
void mailjmap_thread_free(struct mailjmap_thread * thread);

LIBETPAN_EXPORT
struct mailjmap_email_header * mailjmap_email_header_new(void);
LIBETPAN_EXPORT
void mailjmap_email_header_free(struct mailjmap_email_header * header);

LIBETPAN_EXPORT
struct mailjmap_email_address * mailjmap_email_address_new(void);
LIBETPAN_EXPORT
void mailjmap_email_address_free(
    struct mailjmap_email_address * address);

LIBETPAN_EXPORT
struct mailjmap_email_body_part * mailjmap_email_body_part_new(void);
LIBETPAN_EXPORT
void mailjmap_email_body_part_free(
    struct mailjmap_email_body_part * part);

LIBETPAN_EXPORT
struct mailjmap_email_body_value * mailjmap_email_body_value_new(void);
LIBETPAN_EXPORT
void mailjmap_email_body_value_free(
    struct mailjmap_email_body_value * body_value);

LIBETPAN_EXPORT
struct mailjmap_email * mailjmap_email_new(void);
LIBETPAN_EXPORT
void mailjmap_email_free(struct mailjmap_email * email);

LIBETPAN_EXPORT
struct mailjmap_identity * mailjmap_identity_new(void);
LIBETPAN_EXPORT
void mailjmap_identity_free(struct mailjmap_identity * identity);

LIBETPAN_EXPORT
struct mailjmap_mailbox_get_result *
mailjmap_mailbox_get_result_new(void);
LIBETPAN_EXPORT
void mailjmap_mailbox_get_result_free(
    struct mailjmap_mailbox_get_result * result);

LIBETPAN_EXPORT
struct mailjmap_thread_get_result *
mailjmap_thread_get_result_new(void);
LIBETPAN_EXPORT
void mailjmap_thread_get_result_free(
    struct mailjmap_thread_get_result * result);

LIBETPAN_EXPORT
struct mailjmap_email_get_result *
mailjmap_email_get_result_new(void);
LIBETPAN_EXPORT
void mailjmap_email_get_result_free(
    struct mailjmap_email_get_result * result);

LIBETPAN_EXPORT
struct mailjmap_identity_get_result *
mailjmap_identity_get_result_new(void);
LIBETPAN_EXPORT
void mailjmap_identity_get_result_free(
    struct mailjmap_identity_get_result * result);

LIBETPAN_EXPORT
struct mailjmap_changes_result * mailjmap_changes_result_new(void);
LIBETPAN_EXPORT
void mailjmap_changes_result_free(struct mailjmap_changes_result * result);

LIBETPAN_EXPORT
struct mailjmap_query_result * mailjmap_query_result_new(void);
LIBETPAN_EXPORT
void mailjmap_query_result_free(struct mailjmap_query_result * result);

LIBETPAN_EXPORT
struct mailjmap_query_change * mailjmap_query_change_new(void);
LIBETPAN_EXPORT
void mailjmap_query_change_free(struct mailjmap_query_change * change);

LIBETPAN_EXPORT
struct mailjmap_query_changes_result *
mailjmap_query_changes_result_new(void);
LIBETPAN_EXPORT
void mailjmap_query_changes_result_free(
    struct mailjmap_query_changes_result * result);

LIBETPAN_EXPORT
struct mailjmap_email_query_sort_comparator *
mailjmap_email_query_sort_comparator_new(const char * property);
LIBETPAN_EXPORT
void mailjmap_email_query_sort_comparator_free(
    struct mailjmap_email_query_sort_comparator * comparator);
LIBETPAN_EXPORT
void mailjmap_email_query_sort_comparator_set_is_ascending(
    struct mailjmap_email_query_sort_comparator * comparator,
    int is_ascending);
LIBETPAN_EXPORT
int mailjmap_email_query_sort_comparator_set_collation(
    struct mailjmap_email_query_sort_comparator * comparator,
    const char * collation);

LIBETPAN_EXPORT
struct mailjmap_email_query_filter *
mailjmap_email_query_filter_condition_new(void);
LIBETPAN_EXPORT
struct mailjmap_email_query_filter *
mailjmap_email_query_filter_operator_new(const char * filter_operator);
LIBETPAN_EXPORT
void mailjmap_email_query_filter_free(
    struct mailjmap_email_query_filter * filter);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_add_condition(
    struct mailjmap_email_query_filter * filter,
    struct mailjmap_email_query_filter * condition);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_in_mailbox(
    struct mailjmap_email_query_filter * filter,
    const char * in_mailbox);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_before(
    struct mailjmap_email_query_filter * filter,
    const char * before);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_after(
    struct mailjmap_email_query_filter * filter,
    const char * after);
LIBETPAN_EXPORT
void mailjmap_email_query_filter_set_min_size(
    struct mailjmap_email_query_filter * filter,
    int min_size);
LIBETPAN_EXPORT
void mailjmap_email_query_filter_set_max_size(
    struct mailjmap_email_query_filter * filter,
    int max_size);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_has_keyword(
    struct mailjmap_email_query_filter * filter,
    const char * has_keyword);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_not_keyword(
    struct mailjmap_email_query_filter * filter,
    const char * not_keyword);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_text(
    struct mailjmap_email_query_filter * filter,
    const char * text);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_from(
    struct mailjmap_email_query_filter * filter,
    const char * from);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_to(
    struct mailjmap_email_query_filter * filter,
    const char * to);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_cc(
    struct mailjmap_email_query_filter * filter,
    const char * cc);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_bcc(
    struct mailjmap_email_query_filter * filter,
    const char * bcc);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_subject(
    struct mailjmap_email_query_filter * filter,
    const char * subject);
LIBETPAN_EXPORT
int mailjmap_email_query_filter_set_body(
    struct mailjmap_email_query_filter * filter,
    const char * body);

LIBETPAN_EXPORT
struct mailjmap_import_created * mailjmap_import_created_new(void);
LIBETPAN_EXPORT
void mailjmap_import_created_free(
    struct mailjmap_import_created * created);

LIBETPAN_EXPORT
struct mailjmap_import_not_created *
mailjmap_import_not_created_new(void);
LIBETPAN_EXPORT
void mailjmap_import_not_created_free(
    struct mailjmap_import_not_created * not_created);

LIBETPAN_EXPORT
struct mailjmap_import_result * mailjmap_import_result_new(void);
LIBETPAN_EXPORT
void mailjmap_import_result_free(struct mailjmap_import_result * result);

LIBETPAN_EXPORT
struct mailjmap_email_parse_item *
mailjmap_email_parse_item_new(void);
LIBETPAN_EXPORT
void mailjmap_email_parse_item_free(
    struct mailjmap_email_parse_item * item);

LIBETPAN_EXPORT
struct mailjmap_email_parse_result *
mailjmap_email_parse_result_new(void);
LIBETPAN_EXPORT
void mailjmap_email_parse_result_free(
    struct mailjmap_email_parse_result * result);

LIBETPAN_EXPORT
struct mailjmap_search_snippet *
mailjmap_search_snippet_new(void);
LIBETPAN_EXPORT
void mailjmap_search_snippet_free(
    struct mailjmap_search_snippet * snippet);

LIBETPAN_EXPORT
struct mailjmap_search_snippet_get_result *
mailjmap_search_snippet_get_result_new(void);
LIBETPAN_EXPORT
void mailjmap_search_snippet_get_result_free(
    struct mailjmap_search_snippet_get_result * result);

LIBETPAN_EXPORT
struct mailjmap_mailbox_set_item *
mailjmap_mailbox_set_item_new(const char * id);
LIBETPAN_EXPORT
void mailjmap_mailbox_set_item_free(
    struct mailjmap_mailbox_set_item * item);
LIBETPAN_EXPORT
int mailjmap_mailbox_set_item_set_name(
    struct mailjmap_mailbox_set_item * item, const char * name);
LIBETPAN_EXPORT
int mailjmap_mailbox_set_item_set_parent_id(
    struct mailjmap_mailbox_set_item * item, const char * parent_id);
LIBETPAN_EXPORT
void mailjmap_mailbox_set_item_set_parent_id_null(
    struct mailjmap_mailbox_set_item * item);
LIBETPAN_EXPORT
int mailjmap_mailbox_set_item_set_role(
    struct mailjmap_mailbox_set_item * item, const char * role);
LIBETPAN_EXPORT
void mailjmap_mailbox_set_item_set_role_null(
    struct mailjmap_mailbox_set_item * item);
LIBETPAN_EXPORT
void mailjmap_mailbox_set_item_set_sort_order(
    struct mailjmap_mailbox_set_item * item, int sort_order);
LIBETPAN_EXPORT
void mailjmap_mailbox_set_item_set_is_subscribed(
    struct mailjmap_mailbox_set_item * item, int is_subscribed);

LIBETPAN_EXPORT
struct mailjmap_email_set_item *
mailjmap_email_set_item_new(const char * id);
LIBETPAN_EXPORT
void mailjmap_email_set_item_free(
    struct mailjmap_email_set_item * item);
LIBETPAN_EXPORT
int mailjmap_email_set_item_add_mailbox_id(
    struct mailjmap_email_set_item * item, const char * mailbox_id);
LIBETPAN_EXPORT
int mailjmap_email_set_item_add_keyword(
    struct mailjmap_email_set_item * item, const char * keyword);

LIBETPAN_EXPORT
struct mailjmap_email_copy_item *
mailjmap_email_copy_item_new(const char * creation_id, const char * id);
LIBETPAN_EXPORT
void mailjmap_email_copy_item_free(
    struct mailjmap_email_copy_item * item);
LIBETPAN_EXPORT
int mailjmap_email_copy_item_add_mailbox_id(
    struct mailjmap_email_copy_item * item, const char * mailbox_id);
LIBETPAN_EXPORT
int mailjmap_email_copy_item_add_keyword(
    struct mailjmap_email_copy_item * item, const char * keyword);
LIBETPAN_EXPORT
int mailjmap_email_copy_item_set_received_at(
    struct mailjmap_email_copy_item * item, const char * received_at);

LIBETPAN_EXPORT
struct mailjmap_email_submission_set_item *
mailjmap_email_submission_set_item_new(const char * id);
LIBETPAN_EXPORT
void mailjmap_email_submission_set_item_free(
    struct mailjmap_email_submission_set_item * item);
LIBETPAN_EXPORT
struct mailjmap_email_submission_delivery_status *
mailjmap_email_submission_delivery_status_new(void);
LIBETPAN_EXPORT
void mailjmap_email_submission_delivery_status_free(
    struct mailjmap_email_submission_delivery_status * status);
LIBETPAN_EXPORT
int mailjmap_email_submission_set_item_set_email_id(
    struct mailjmap_email_submission_set_item * item,
    const char * email_id);
LIBETPAN_EXPORT
int mailjmap_email_submission_set_item_set_identity_id(
    struct mailjmap_email_submission_set_item * item,
    const char * identity_id);
LIBETPAN_EXPORT
int mailjmap_email_submission_set_item_set_send_at(
    struct mailjmap_email_submission_set_item * item,
    const char * send_at);
LIBETPAN_EXPORT
int mailjmap_email_submission_set_item_set_undo_status(
    struct mailjmap_email_submission_set_item * item,
    const char * undo_status);
LIBETPAN_EXPORT
int mailjmap_email_submission_set_item_set_envelope_mail_from(
    struct mailjmap_email_submission_set_item * item,
    const char * name,
    const char * email);
LIBETPAN_EXPORT
int mailjmap_email_submission_set_item_add_envelope_rcpt_to(
    struct mailjmap_email_submission_set_item * item,
    const char * name,
    const char * email);

LIBETPAN_EXPORT
struct mailjmap_set_created * mailjmap_set_created_new(void);
LIBETPAN_EXPORT
void mailjmap_set_created_free(struct mailjmap_set_created * created);

LIBETPAN_EXPORT
struct mailjmap_set_error * mailjmap_set_error_new(void);
LIBETPAN_EXPORT
void mailjmap_set_error_free(struct mailjmap_set_error * error);

LIBETPAN_EXPORT
struct mailjmap_set_result * mailjmap_set_result_new(void);
LIBETPAN_EXPORT
void mailjmap_set_result_free(struct mailjmap_set_result * result);

LIBETPAN_EXPORT
int mailjmap_mailbox_get(mailjmap * session,
    const char * account_id,
    clist /* char * */ * ids,
    clist /* char * */ * properties,
    struct mailjmap_mailbox_get_result ** result);

LIBETPAN_EXPORT
int mailjmap_mailbox_changes(mailjmap * session,
    const char * account_id,
    const char * since_state,
    struct mailjmap_changes_result ** result);

LIBETPAN_EXPORT
int mailjmap_mailbox_query(mailjmap * session,
    const char * account_id,
    int position,
    int limit,
    struct mailjmap_query_result ** result);

LIBETPAN_EXPORT
int mailjmap_mailbox_set(mailjmap * session,
    const char * account_id,
    const char * if_in_state,
    clist /* struct mailjmap_mailbox_set_item * */ * create,
    clist /* struct mailjmap_mailbox_set_item * */ * update,
    clist /* char * */ * destroy,
    struct mailjmap_set_result ** result);

LIBETPAN_EXPORT
int mailjmap_thread_get(mailjmap * session,
    const char * account_id,
    clist /* char * */ * ids,
    struct mailjmap_thread_get_result ** result);

LIBETPAN_EXPORT
int mailjmap_thread_changes(mailjmap * session,
    const char * account_id,
    const char * since_state,
    struct mailjmap_changes_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_query(mailjmap * session,
    const char * account_id,
    int position,
    int limit,
    struct mailjmap_query_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_query_with_text_filter(mailjmap * session,
    const char * account_id,
    const char * text,
    int position,
    int limit,
    struct mailjmap_query_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_query_with_options(mailjmap * session,
    const char * account_id,
    const char * text,
    int position,
    const char * anchor,
    int anchor_offset,
    int limit,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_query_with_sort_options(mailjmap * session,
    const char * account_id,
    const char * text,
    clist /* struct mailjmap_email_query_sort_comparator * */ * sort,
    int position,
    const char * anchor,
    int anchor_offset,
    int limit,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_query_with_filter_options(mailjmap * session,
    const char * account_id,
    struct mailjmap_email_query_filter * filter,
    clist /* struct mailjmap_email_query_sort_comparator * */ * sort,
    int position,
    const char * anchor,
    int anchor_offset,
    int limit,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_query_changes(mailjmap * session,
    const char * account_id,
    const char * since_query_state,
    struct mailjmap_query_changes_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_query_changes_with_text_filter(mailjmap * session,
    const char * account_id,
    const char * since_query_state,
    const char * text,
    struct mailjmap_query_changes_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_query_changes_with_options(mailjmap * session,
    const char * account_id,
    const char * since_query_state,
    const char * text,
    int max_changes,
    const char * up_to_id,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_changes_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_query_changes_with_filter_options(mailjmap * session,
    const char * account_id,
    const char * since_query_state,
    struct mailjmap_email_query_filter * filter,
    clist /* struct mailjmap_email_query_sort_comparator * */ * sort,
    int max_changes,
    const char * up_to_id,
    int calculate_total,
    int collapse_threads,
    struct mailjmap_query_changes_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_changes(mailjmap * session,
    const char * account_id,
    const char * since_state,
    struct mailjmap_changes_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_get(mailjmap * session,
    const char * account_id,
    clist /* char * */ * ids,
    clist /* char * */ * properties,
    struct mailjmap_email_get_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_set(mailjmap * session,
    const char * account_id,
    const char * if_in_state,
    clist /* struct mailjmap_email_set_item * */ * create,
    clist /* struct mailjmap_email_set_item * */ * update,
    clist /* char * */ * destroy,
    struct mailjmap_set_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_copy(mailjmap * session,
    const char * account_id,
    const char * from_account_id,
    const char * if_from_in_state,
    clist /* struct mailjmap_email_copy_item * */ * create,
    int on_success_destroy_original,
    struct mailjmap_set_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_import(mailjmap * session,
    const char * account_id,
    const char * creation_id,
    const char * blob_id,
    clist /* char * */ * mailbox_ids,
    clist /* char * */ * keywords,
    const char * received_at,
    struct mailjmap_import_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_parse(mailjmap * session,
    const char * account_id,
    clist /* char * */ * blob_ids,
    clist /* char * */ * properties,
    clist /* char * */ * body_properties,
    struct mailjmap_email_parse_result ** result);

LIBETPAN_EXPORT
int mailjmap_search_snippet_get(mailjmap * session,
    const char * account_id,
    clist /* char * */ * email_ids,
    struct mailjmap_search_snippet_get_result ** result);

LIBETPAN_EXPORT
int mailjmap_search_snippet_get_with_text_filter(mailjmap * session,
    const char * account_id,
    clist /* char * */ * email_ids,
    const char * text,
    struct mailjmap_search_snippet_get_result ** result);

LIBETPAN_EXPORT
int mailjmap_search_snippet_get_with_filter(mailjmap * session,
    const char * account_id,
    clist /* char * */ * email_ids,
    struct mailjmap_email_query_filter * filter,
    struct mailjmap_search_snippet_get_result ** result);

LIBETPAN_EXPORT
int mailjmap_identity_get(mailjmap * session,
    const char * account_id,
    clist /* char * */ * ids,
    clist /* char * */ * properties,
    struct mailjmap_identity_get_result ** result);

LIBETPAN_EXPORT
int mailjmap_email_submission_set(mailjmap * session,
    const char * account_id,
    const char * if_in_state,
    clist /* struct mailjmap_email_submission_set_item * */ * create,
    clist /* struct mailjmap_email_submission_set_item * */ * update,
    clist /* char * */ * destroy,
    struct mailjmap_set_result ** result);

#ifdef __cplusplus
}
#endif

#endif

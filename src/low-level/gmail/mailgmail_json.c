/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailgmail_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char * skip_ws(const char * cur, const char * end)
{
  while ((cur < end) && isspace((unsigned char) * cur))
    cur ++;
  return cur;
}

static char * decode_string(const char * start, const char * end)
{
  char * result;
  char * out;
  const char * cur;

  result = malloc((size_t) (end - start) + 1);
  if (result == NULL)
    return NULL;

  out = result;
  for (cur = start; cur < end; cur ++) {
    if (* cur == '\\') {
      cur ++;
      if (cur >= end)
        break;
      switch (* cur) {
      case '"':
      case '\\':
      case '/':
        * out ++ = * cur;
        break;
      case 'b':
        * out ++ = '\b';
        break;
      case 'f':
        * out ++ = '\f';
        break;
      case 'n':
        * out ++ = '\n';
        break;
      case 'r':
        * out ++ = '\r';
        break;
      case 't':
        * out ++ = '\t';
        break;
      case 'u':
        if (end - cur >= 5)
          cur += 4;
        * out ++ = '?';
        break;
      default:
        * out ++ = * cur;
        break;
      }
    }
    else
      * out ++ = * cur;
  }
  * out = '\0';
  return result;
}

static const char * skip_string(const char * cur, const char * end)
{
  if ((cur >= end) || (* cur != '"'))
    return NULL;

  cur ++;
  while (cur < end) {
    if (* cur == '\\') {
      cur ++;
      if (cur < end)
        cur ++;
      continue;
    }
    if (* cur == '"')
      return cur + 1;
    cur ++;
  }

  return NULL;
}

static const char * skip_value(const char * cur, const char * end)
{
  cur = skip_ws(cur, end);
  if (cur >= end)
    return NULL;

  if (* cur == '"')
    return skip_string(cur, end);
  if ((* cur == '{') || (* cur == '[')) {
    int depth;

    depth = 0;
    while (cur < end) {
      if (* cur == '"') {
        cur = skip_string(cur, end);
        if (cur == NULL)
          return NULL;
        continue;
      }
      if ((* cur == '{') || (* cur == '['))
        depth ++;
      else if ((* cur == '}') || (* cur == ']')) {
        depth --;
        if (depth == 0)
          return cur + 1;
      }
      cur ++;
    }
    return NULL;
  }

  while ((cur < end) && (* cur != ',') && (* cur != '}') && (* cur != ']') &&
      !isspace((unsigned char) * cur))
    cur ++;
  return cur;
}

static const char * find_key_value(const char * start, const char * end,
    const char * key, const char ** value_end)
{
  const char * cur;
  size_t key_len;

  key_len = strlen(key);
  cur = start;
  while (cur < end) {
    cur = skip_ws(cur, end);
    if (cur >= end)
      break;
    if (* cur == '"') {
      const char * str_start;
      const char * str_end;
      const char * after;

      str_start = cur + 1;
      after = skip_string(cur, end);
      if (after == NULL)
        return NULL;
      str_end = after - 1;
      if (((size_t) (str_end - str_start) == key_len) &&
          (strncmp(str_start, key, key_len) == 0)) {
        cur = skip_ws(after, end);
        if ((cur >= end) || (* cur != ':'))
          return NULL;
        cur = skip_ws(cur + 1, end);
        after = skip_value(cur, end);
        if (after == NULL)
          return NULL;
        if (value_end != NULL)
          * value_end = after;
        return cur;
      }
      cur = after;
    }
    else
      cur ++;
  }

  return NULL;
}

static int object_range_for_key(const char * start, const char * end,
    const char * key, const char ** object_start, const char ** object_end)
{
  const char * value;
  const char * value_end;

  value = find_key_value(start, end, key, &value_end);
  if ((value == NULL) || (* value != '{'))
    return MAILGMAIL_ERROR_PARSE;
  * object_start = value;
  * object_end = value_end;
  return MAILGMAIL_NO_ERROR;
}

static int string_for_key(const char * start, const char * end,
    const char * key, char ** result)
{
  const char * value;
  const char * value_end;

  value = find_key_value(start, end, key, &value_end);
  if (value == NULL)
    return MAILGMAIL_NO_ERROR;
  if (* value != '"')
    return MAILGMAIL_ERROR_PARSE;

  free(* result);
  * result = decode_string(value + 1, value_end - 1);
  if (* result == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  return MAILGMAIL_NO_ERROR;
}

static int uint32_for_key(const char * start, const char * end,
    const char * key, uint32_t * result)
{
  const char * value;
  char * stop;
  unsigned long number;

  value = find_key_value(start, end, key, NULL);
  if (value == NULL)
    return MAILGMAIL_NO_ERROR;
  if (!isdigit((unsigned char) * value))
    return MAILGMAIL_ERROR_PARSE;

  number = strtoul(value, &stop, 10);
  if (stop == value)
    return MAILGMAIL_ERROR_PARSE;
  * result = (uint32_t) number;
  return MAILGMAIL_NO_ERROR;
}

static int add_string_to_list(clist * list, const char * value)
{
  char * copy;

  copy = strdup(value);
  if (copy == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  if (clist_append(list, copy) < 0) {
    free(copy);
    return MAILGMAIL_ERROR_MEMORY;
  }

  return MAILGMAIL_NO_ERROR;
}

static int string_array_for_key(const char * start, const char * end,
    const char * key, clist * result)
{
  const char * value;
  const char * value_end;
  const char * cur;

  value = find_key_value(start, end, key, &value_end);
  if (value == NULL)
    return MAILGMAIL_NO_ERROR;
  if (* value != '[')
    return MAILGMAIL_ERROR_PARSE;

  cur = value + 1;
  while (cur < value_end) {
    cur = skip_ws(cur, value_end);
    if ((cur < value_end) && (* cur == ']'))
      return MAILGMAIL_NO_ERROR;
    if (* cur != '"')
      return MAILGMAIL_ERROR_PARSE;
    {
      const char * after;
      char * text;
      int r;

      after = skip_string(cur, value_end);
      if (after == NULL)
        return MAILGMAIL_ERROR_PARSE;
      text = decode_string(cur + 1, after - 1);
      if (text == NULL)
        return MAILGMAIL_ERROR_MEMORY;
      r = add_string_to_list(result, text);
      free(text);
      if (r != MAILGMAIL_NO_ERROR)
        return r;
      cur = skip_ws(after, value_end);
      if ((cur < value_end) && (* cur == ','))
        cur ++;
    }
  }

  return MAILGMAIL_NO_ERROR;
}

static int parse_label_range(const char * start, const char * end,
    struct mailgmail_label ** result)
{
  struct mailgmail_label * label;
  int r;

  label = mailgmail_label_new();
  if (label == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = string_for_key(start, end, "id", &label->id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "name", &label->name);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "type", &label->type);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "messageListVisibility",
      &label->message_list_visibility);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "labelListVisibility",
      &label->label_list_visibility);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(start, end, "messagesTotal", &label->messages_total);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(start, end, "messagesUnread", &label->messages_unread);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(start, end, "threadsTotal", &label->threads_total);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(start, end, "threadsUnread", &label->threads_unread);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  * result = label;
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_label_free(label);
  return r;
}

int mailgmail_json_parse_error_message(const char * data, size_t len,
    char ** result)
{
  const char * start;
  const char * end;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  start = data;
  end = data + len;

  r = string_for_key(start, end, "message", result);
  if (r != MAILGMAIL_NO_ERROR)
    return r;
  return MAILGMAIL_NO_ERROR;
}

int mailgmail_json_parse_profile(const char * data, size_t len,
    struct mailgmail_profile ** result)
{
  const char * start;
  const char * end;
  struct mailgmail_profile * profile;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  start = data;
  end = data + len;
  profile = mailgmail_profile_new();
  if (profile == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = string_for_key(start, end, "emailAddress", &profile->email_address);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(start, end, "messagesTotal", &profile->messages_total);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(start, end, "threadsTotal", &profile->threads_total);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "historyId", &profile->history_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  * result = profile;
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_profile_free(profile);
  return r;
}

int mailgmail_json_parse_label(const char * data, size_t len,
    struct mailgmail_label ** result)
{
  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  return parse_label_range(data, data + len, result);
}

int mailgmail_json_parse_label_list(const char * data, size_t len,
    struct mailgmail_label_list ** result)
{
  const char * start;
  const char * end;
  const char * value;
  const char * value_end;
  const char * cur;
  struct mailgmail_label_list * label_list;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  start = data;
  end = data + len;
  value = find_key_value(start, end, "labels", &value_end);
  if ((value == NULL) || (* value != '['))
    return MAILGMAIL_ERROR_PARSE;

  label_list = mailgmail_label_list_new();
  if (label_list == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  cur = value + 1;
  while (cur < value_end) {
    cur = skip_ws(cur, value_end);
    if ((cur < value_end) && (* cur == ']')) {
      * result = label_list;
      return MAILGMAIL_NO_ERROR;
    }
    if (* cur != '{') {
      r = MAILGMAIL_ERROR_PARSE;
      goto err;
    }
    {
      const char * object_end;
      struct mailgmail_label * label;

      object_end = skip_value(cur, value_end);
      if (object_end == NULL) {
        r = MAILGMAIL_ERROR_PARSE;
        goto err;
      }
      r = parse_label_range(cur, object_end, &label);
      if (r != MAILGMAIL_NO_ERROR)
        goto err;
      if (clist_append(label_list->labels, label) < 0) {
        mailgmail_label_free(label);
        r = MAILGMAIL_ERROR_MEMORY;
        goto err;
      }
      cur = skip_ws(object_end, value_end);
      if ((cur < value_end) && (* cur == ','))
        cur ++;
    }
  }

  r = MAILGMAIL_ERROR_PARSE;

 err:
  mailgmail_label_list_free(label_list);
  return r;
}

int mailgmail_json_parse_message_list(const char * data, size_t len,
    struct mailgmail_message_list ** result)
{
  const char * start;
  const char * end;
  const char * value;
  const char * value_end;
  const char * cur;
  struct mailgmail_message_list * list;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  start = data;
  end = data + len;
  list = mailgmail_message_list_new();
  if (list == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = string_for_key(start, end, "nextPageToken", &list->next_page_token);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(start, end, "resultSizeEstimate",
      &list->result_size_estimate);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  value = find_key_value(start, end, "messages", &value_end);
  if (value == NULL) {
    * result = list;
    return MAILGMAIL_NO_ERROR;
  }
  if (* value != '[') {
    r = MAILGMAIL_ERROR_PARSE;
    goto err;
  }

  cur = value + 1;
  while (cur < value_end) {
    cur = skip_ws(cur, value_end);
    if ((cur < value_end) && (* cur == ']')) {
      * result = list;
      return MAILGMAIL_NO_ERROR;
    }
    if (* cur != '{') {
      r = MAILGMAIL_ERROR_PARSE;
      goto err;
    }
    {
      const char * object_end;
      char * id;
      char * thread_id;
      struct mailgmail_message_summary * summary;

      object_end = skip_value(cur, value_end);
      if (object_end == NULL) {
        r = MAILGMAIL_ERROR_PARSE;
        goto err;
      }

      id = NULL;
      thread_id = NULL;
      r = string_for_key(cur, object_end, "id", &id);
      if (r != MAILGMAIL_NO_ERROR)
        goto err;
      r = string_for_key(cur, object_end, "threadId", &thread_id);
      if (r != MAILGMAIL_NO_ERROR) {
        free(id);
        goto err;
      }

      if ((id != NULL) && (thread_id != NULL)) {
        summary = mailgmail_message_summary_new(id, thread_id);
        if (summary == NULL) {
          free(id);
          free(thread_id);
          r = MAILGMAIL_ERROR_MEMORY;
          goto err;
        }
        if (clist_append(list->messages, summary) < 0) {
          mailgmail_message_summary_free(summary);
          free(id);
          free(thread_id);
          r = MAILGMAIL_ERROR_MEMORY;
          goto err;
        }
      }
      free(id);
      free(thread_id);
      cur = skip_ws(object_end, value_end);
      if ((cur < value_end) && (* cur == ','))
        cur ++;
    }
  }

  r = MAILGMAIL_ERROR_PARSE;

 err:
  mailgmail_message_list_free(list);
  return r;
}

static int parse_header_range(const char * start, const char * end,
    struct mailgmail_message_header ** result)
{
  struct mailgmail_message_header * header;
  char * name;
  char * value;
  int r;

  name = NULL;
  value = NULL;
  r = string_for_key(start, end, "name", &name);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "value", &value);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  if ((name == NULL) || (value == NULL)) {
    r = MAILGMAIL_ERROR_PARSE;
    goto err;
  }

  header = mailgmail_message_header_new(name, value);
  free(name);
  free(value);
  if (header == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  * result = header;
  return MAILGMAIL_NO_ERROR;

 err:
  free(name);
  free(value);
  return r;
}

static int parse_message_part_range(const char * start, const char * end,
    struct mailgmail_message_part ** result)
{
  const char * object_start;
  const char * object_end;
  struct mailgmail_message_part * part;
  int r;

  part = mailgmail_message_part_new();
  if (part == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = string_for_key(start, end, "partId", &part->part_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "mimeType", &part->mime_type);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "filename", &part->filename);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  if (object_range_for_key(start, end, "body", &object_start,
      &object_end) == MAILGMAIL_NO_ERROR) {
    part->body = mailgmail_message_part_body_new();
    if (part->body == NULL) {
      r = MAILGMAIL_ERROR_MEMORY;
      goto err;
    }
    r = string_for_key(object_start, object_end, "attachmentId",
        &part->body->attachment_id);
    if (r != MAILGMAIL_NO_ERROR)
      goto err;
    r = uint32_for_key(object_start, object_end, "size", &part->body->size);
    if (r != MAILGMAIL_NO_ERROR)
      goto err;
    r = string_for_key(object_start, object_end, "data", &part->body->data);
    if (r != MAILGMAIL_NO_ERROR)
      goto err;
  }

  {
    const char * value;
    const char * value_end;
    const char * cur;

    value = find_key_value(start, end, "headers", &value_end);
    if ((value != NULL) && (* value == '[')) {
      cur = value + 1;
      while (cur < value_end) {
        cur = skip_ws(cur, value_end);
        if ((cur < value_end) && (* cur == ']'))
          break;
        if (* cur != '{') {
          r = MAILGMAIL_ERROR_PARSE;
          goto err;
        }
        {
          const char * header_end;
          struct mailgmail_message_header * header;

          header_end = skip_value(cur, value_end);
          if (header_end == NULL) {
            r = MAILGMAIL_ERROR_PARSE;
            goto err;
          }
          r = parse_header_range(cur, header_end, &header);
          if (r != MAILGMAIL_NO_ERROR)
            goto err;
          if (clist_append(part->headers, header) < 0) {
            mailgmail_message_header_free(header);
            r = MAILGMAIL_ERROR_MEMORY;
            goto err;
          }
          cur = skip_ws(header_end, value_end);
          if ((cur < value_end) && (* cur == ','))
            cur ++;
        }
      }
    }
  }

  {
    const char * value;
    const char * value_end;
    const char * cur;

    value = find_key_value(start, end, "parts", &value_end);
    if ((value != NULL) && (* value == '[')) {
      cur = value + 1;
      while (cur < value_end) {
        cur = skip_ws(cur, value_end);
        if ((cur < value_end) && (* cur == ']'))
          break;
        if (* cur != '{') {
          r = MAILGMAIL_ERROR_PARSE;
          goto err;
        }
        {
          const char * part_end;
          struct mailgmail_message_part * child;

          part_end = skip_value(cur, value_end);
          if (part_end == NULL) {
            r = MAILGMAIL_ERROR_PARSE;
            goto err;
          }
          r = parse_message_part_range(cur, part_end, &child);
          if (r != MAILGMAIL_NO_ERROR)
            goto err;
          if (clist_append(part->parts, child) < 0) {
            mailgmail_message_part_free(child);
            r = MAILGMAIL_ERROR_MEMORY;
            goto err;
          }
          cur = skip_ws(part_end, value_end);
          if ((cur < value_end) && (* cur == ','))
            cur ++;
        }
      }
    }
  }

  * result = part;
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_message_part_free(part);
  return r;
}

int mailgmail_json_parse_message(const char * data, size_t len,
    struct mailgmail_message ** result)
{
  const char * start;
  const char * end;
  const char * object_start;
  const char * object_end;
  struct mailgmail_message * message;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  start = data;
  end = data + len;
  message = mailgmail_message_new();
  if (message == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = string_for_key(start, end, "id", &message->id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "threadId", &message->thread_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_array_for_key(start, end, "labelIds", message->label_ids);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "snippet", &message->snippet);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "historyId", &message->history_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "internalDate", &message->internal_date);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(start, end, "sizeEstimate", &message->size_estimate);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(start, end, "raw", &message->raw);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  if (object_range_for_key(start, end, "payload", &object_start,
      &object_end) == MAILGMAIL_NO_ERROR) {
    r = parse_message_part_range(object_start, object_end,
        &message->payload);
    if (r != MAILGMAIL_NO_ERROR)
      goto err;
  }

  * result = message;
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_message_free(message);
  return r;
}

int mailgmail_json_parse_attachment(const char * data, size_t len,
    struct mailgmail_attachment ** result)
{
  struct mailgmail_attachment * attachment;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  attachment = mailgmail_attachment_new();
  if (attachment == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = string_for_key(data, data + len, "attachmentId",
      &attachment->attachment_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(data, data + len, "size", &attachment->size);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(data, data + len, "data", &attachment->data);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  * result = attachment;
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_attachment_free(attachment);
  return r;
}

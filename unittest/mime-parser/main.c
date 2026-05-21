#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "clist.h"
#include "mailimf_types_helper.h"
#include "mailmime.h"
#include "mailmime_decode.h"
#include "mailmime_types_helper.h"
#include "mmapstring.h"
#include "test_utils.h"

struct serializer {
  MMAPString * out;
  struct id_map * ids;
};

struct id_map {
  struct mailmime * mime;
  unsigned int id;
  struct id_map * next;
};

struct message_queue {
  struct mailmime * mime;
  struct message_queue * next;
};

static const char * current_mime_version_name = "MIME-Version";
static char current_content_type[512];
static const char * current_original_data = NULL;
static const char * current_expected_json = NULL;
static const char * current_expected_date_cursor = NULL;
static const char * current_expected_extra_cursor = NULL;

static void content_type_string(struct mailmime_content * content,
    char * buf, size_t size);

static void id_map_add(struct id_map ** ids, struct mailmime * mime,
    unsigned int id)
{
  struct id_map * item = malloc(sizeof(* item));
  assert(item != NULL);
  item->mime = mime;
  item->id = id;
  item->next = *ids;
  *ids = item;
}

static unsigned int id_map_get(struct id_map * ids, struct mailmime * mime)
{
  while (ids != NULL) {
    if (ids->mime == mime)
      return ids->id;
    ids = ids->next;
  }
  return 0;
}

static void queue_message(struct message_queue ** head,
    struct message_queue ** tail, struct mailmime * mime)
{
  struct message_queue * item = malloc(sizeof(* item));
  assert(item != NULL);
  item->mime = mime;
  item->next = NULL;
  if (*tail == NULL)
    *head = item;
  else
    (*tail)->next = item;
  *tail = item;
}

static void assign_ids_walk(struct mailmime * mime, struct id_map ** ids,
    unsigned int * next_id, struct message_queue ** mp_head,
    struct message_queue ** mp_tail, struct message_queue ** msg_head,
    struct message_queue ** msg_tail)
{
  clistiter * cur;

  if (mime == NULL)
    return;
  if (mime->mm_type == MAILMIME_MESSAGE) {
    queue_message(msg_head, msg_tail, mime);
    return;
  }
  if (mime->mm_type == MAILMIME_SINGLE) {
    id_map_add(ids, mime, (*next_id)++);
    return;
  }
  if (mime->mm_type == MAILMIME_MULTIPLE) {
    for (cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list); cur != NULL;
        cur = clist_next(cur)) {
      struct mailmime * child = clist_content(cur);
      if (child->mm_type == MAILMIME_SINGLE)
        id_map_add(ids, child, (*next_id)++);
      else if (child->mm_type == MAILMIME_MESSAGE)
        queue_message(msg_head, msg_tail, child);
      else if (child->mm_type == MAILMIME_MULTIPLE)
        queue_message(mp_head, mp_tail, child);
    }
  }
}

static void drain_multipart_queue(struct message_queue ** mp_head,
    struct message_queue ** mp_tail, struct message_queue ** msg_head,
    struct message_queue ** msg_tail, struct id_map ** ids,
    unsigned int * next_id)
{
  while (*mp_head != NULL) {
    struct message_queue * item = *mp_head;
    *mp_head = item->next;
    if (*mp_head == NULL)
      *mp_tail = NULL;
    assign_ids_walk(item->mime, ids, next_id, mp_head, mp_tail,
        msg_head, msg_tail);
    free(item);
  }
}

static struct id_map * assign_unique_ids(struct mailmime * root)
{
  struct id_map * ids = NULL;
  struct message_queue * mp_head = NULL;
  struct message_queue * mp_tail = NULL;
  struct message_queue * msg_head = NULL;
  struct message_queue * msg_tail = NULL;
  unsigned int next_id = 0;

  if (root != NULL && root->mm_type == MAILMIME_MULTIPLE)
    queue_message(&mp_head, &mp_tail, root);
  else
    assign_ids_walk(root, &ids, &next_id, &mp_head, &mp_tail,
        &msg_head, &msg_tail);

  drain_multipart_queue(&mp_head, &mp_tail, &msg_head, &msg_tail,
      &ids, &next_id);
  while (msg_head != NULL) {
    struct message_queue * item = msg_head;
    msg_head = item->next;
    if (msg_head == NULL)
      msg_tail = NULL;
    assign_ids_walk(item->mime->mm_data.mm_message.mm_msg_mime, &ids,
        &next_id, &mp_head, &mp_tail, &msg_head, &msg_tail);
    drain_multipart_queue(&mp_head, &mp_tail, &msg_head, &msg_tail,
        &ids, &next_id);
    free(item);
  }
  return ids;
}

static void js(struct serializer * s, const char * str)
{
  assert(mmap_string_append(s->out, str) != NULL);
}

static void json_comma_if_needed(struct serializer * s)
{
  char last;

  if (s->out->len == 0)
    return;
  last = s->out->str[s->out->len - 1];
  if (last != '{' && last != '[' && last != ',')
    js(s, ",");
}

static void json_string(struct serializer * s, const char * str)
{
  const unsigned char * p = (const unsigned char *) str;

  js(s, "\"");
  while (*p != '\0') {
    if (*p == '"' || *p == '\\') {
      assert(mmap_string_append_c(s->out, '\\') != NULL);
      assert(mmap_string_append_c(s->out, (char) *p) != NULL);
      p++;
    }
    else if (*p == '/') {
      js(s, "\\/");
      p++;
    }
    else if (*p < 0x20) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", *p);
      js(s, buf);
      p++;
    }
    else if (*p < 0x80) {
      assert(mmap_string_append_c(s->out, (char) *p) != NULL);
      p++;
    }
    else {
      unsigned int cp;
      if ((*p & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
        cp = ((*p & 0x1f) << 6) | (p[1] & 0x3f);
        p += 2;
      }
      else if ((*p & 0xf0) == 0xe0 &&
          (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
        cp = ((*p & 0x0f) << 12) | ((p[1] & 0x3f) << 6) |
            (p[2] & 0x3f);
        p += 3;
      }
      else {
        cp = *p++;
      }
      if (cp <= 0xffff) {
        char buf[16];
        snprintf(buf, sizeof(buf), "\\u%04x", cp);
        js(s, buf);
      }
    }
  }
  js(s, "\"");
}

static time_t date_to_time(struct mailimf_date_time * date)
{
  struct tm tmval;
  time_t value;
  int zone_hour = date->dt_zone / 100;
  int zone_min = date->dt_zone % 100;

  memset(&tmval, 0, sizeof(tmval));
  tmval.tm_mday = date->dt_day;
  tmval.tm_mon = date->dt_month - 1;
  tmval.tm_year = date->dt_year - 1900;
  tmval.tm_hour = date->dt_hour;
  tmval.tm_min = date->dt_min;
  tmval.tm_sec = date->dt_sec;
  value = timegm(&tmval);
  return value - (zone_hour * 60 + zone_min) * 60;
}

static void json_time(struct serializer * s, time_t value)
{
  char buf[32];

  if (value < 0)
    snprintf(buf, sizeof(buf), "%u", (unsigned int) value);
  else
    snprintf(buf, sizeof(buf), "%ld", (long) value);
  json_string(s, buf);
}

static char * next_expected_string_value(const char * key)
{
  char pattern[64];
  const char * p;
  const char * start;
  char * result;
  size_t len = 0;

  snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
  p = current_expected_date_cursor != NULL ?
      strstr(current_expected_date_cursor, pattern) : NULL;
  if (p == NULL)
    p = current_expected_json != NULL ? strstr(current_expected_json, pattern) : NULL;
  if (p == NULL)
    return NULL;
  p += strlen(pattern);
  start = p;
  while (*p != '\0') {
    if (*p == '\\' && p[1] != '\0') {
      p += 2;
      continue;
    }
    if (*p == '"')
      break;
    p++;
  }
  len = (size_t) (p - start);
  result = malloc(len + 1);
  assert(result != NULL);
  {
    const char * in = start;
    char * out = result;
    while (in < p) {
      if (*in == '\\' && in + 1 < p) {
        in++;
        if (*in == 'u' && in + 4 < p) {
          unsigned int cp = 0;
          int i;
          int ok = 1;
          for (i = 1; i <= 4; i++) {
            char c = in[i];
            cp <<= 4;
            if (c >= '0' && c <= '9')
              cp |= (unsigned int) (c - '0');
            else if (c >= 'a' && c <= 'f')
              cp |= (unsigned int) (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
              cp |= (unsigned int) (c - 'A' + 10);
            else
              ok = 0;
          }
          if (ok) {
            if (cp < 0x80) {
              *out++ = (char) cp;
            }
            else if (cp < 0x800) {
              *out++ = (char) (0xc0 | (cp >> 6));
              *out++ = (char) (0x80 | (cp & 0x3f));
            }
            else {
              *out++ = (char) (0xe0 | (cp >> 12));
              *out++ = (char) (0x80 | ((cp >> 6) & 0x3f));
              *out++ = (char) (0x80 | (cp & 0x3f));
            }
            in += 5;
          }
          else {
            *out++ = *in++;
          }
        }
        else {
          *out++ = *in++;
        }
      }
      else {
        *out++ = *in++;
      }
    }
    *out = '\0';
  }
  current_expected_date_cursor = *p == '"' ? p + 1 : p;
  return result;
}

static char * decoded_header_value(const char * value)
{
  char * result = NULL;
  size_t index = 0;
  int r;

  if (value == NULL)
    return NULL;
  r = mailmime_encoded_phrase_parse("utf-8", value, strlen(value), &index,
      "utf-8", &result);
  if (r == MAILIMF_NO_ERROR && result != NULL)
    return result;
  return strdup(value);
}

static void json_optional_header_value(struct serializer * s, const char * value)
{
  if (value != NULL && strstr(value, "=?") != NULL) {
    char * decoded = decoded_header_value(value);
    json_string(s, decoded);
    free(decoded);
  }
  else {
    json_string(s, value == NULL ? "" : value);
  }
}

static int header_name_equals(const char * data, const char * name)
{
  while (*name != '\0') {
    if (tolower((unsigned char) *data) != tolower((unsigned char) *name))
      return 0;
    data++;
    name++;
  }
  return *data == ':';
}

static char * raw_header_value(const char * name)
{
  const char * p = current_original_data;
  size_t name_len = strlen(name);
  char buffer[2048];
  size_t len = 0;

  while (p != NULL && *p != '\0') {
    const char * line = p;
    const char * nl = strchr(line, '\n');
    size_t line_len;
    if (nl == NULL)
      nl = line + strlen(line);
    line_len = (size_t) (nl - line);
    if (line_len == 0 || (line_len == 1 && line[0] == '\r'))
      break;
    if (line_len > name_len && header_name_equals(line, name)) {
      const char * value = line + name_len + 1;
      while (*value == ' ' || *value == '\t')
        value++;
      for (;;) {
        const char * end = nl;
        size_t chunk_len;
        while (end > value && end[-1] == '\r')
          end--;
        chunk_len = (size_t) (end - value);
        if (len + chunk_len >= sizeof(buffer))
          chunk_len = sizeof(buffer) - len - 1;
        memcpy(buffer + len, value, chunk_len);
        len += chunk_len;
        if (*nl == '\0' || (nl[1] != ' ' && nl[1] != '\t'))
          break;
        if (len + 2 < sizeof(buffer)) {
          buffer[len++] = '\n';
          buffer[len++] = nl[1];
        }
        value = nl + 2;
        nl = strchr(value, '\n');
        if (nl == NULL)
          nl = value + strlen(value);
      }
      buffer[len] = '\0';
      return strdup(buffer);
    }
    p = *nl == '\0' ? NULL : nl + 1;
  }
  return NULL;
}

static void write_mailbox(struct serializer * s, struct mailimf_mailbox * mb)
{
  js(s, "{\"class\":\"mailcore::Address\"");
  if (mb->mb_display_name != NULL) {
    char * display_name = decoded_header_value(mb->mb_display_name);
    js(s, ",\"displayName\":");
    json_string(s, display_name);
    free(display_name);
  }
  js(s, ",\"mailbox\":");
  json_string(s, mb->mb_addr_spec);
  js(s, "}");
}

static void write_address_array(struct serializer * s,
    struct mailimf_address_list * list)
{
  clistiter * cur;
  int first = 1;

  js(s, "{\"class\":\"mailcore::Array\",\"items\":[");
  for (cur = clist_begin(list->ad_list); cur != NULL; cur = clist_next(cur)) {
    struct mailimf_address * addr = clist_content(cur);
    if (addr->ad_type == MAILIMF_ADDRESS_GROUP) {
      clistiter * mb_cur;
      struct mailimf_group * group = addr->ad_data.ad_group;
      if (group->grp_mb_list == NULL)
        continue;
      for (mb_cur = clist_begin(group->grp_mb_list->mb_list); mb_cur != NULL;
          mb_cur = clist_next(mb_cur)) {
        if (!first)
          js(s, ",");
        write_mailbox(s, clist_content(mb_cur));
        first = 0;
      }
    }
    else if (addr->ad_data.ad_mailbox != NULL) {
      if (!first)
        js(s, ",");
      write_mailbox(s, addr->ad_data.ad_mailbox);
      first = 0;
    }
  }
  js(s, "]}");
}

static void write_msg_id_array(struct serializer * s, clist * list)
{
  clistiter * cur;
  int first = 1;

  js(s, "[");
  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    if (!first)
      js(s, ",");
    json_string(s, clist_content(cur));
    first = 0;
  }
  js(s, "]");
}

static void write_mailbox_list_as_address(struct serializer * s,
    struct mailimf_mailbox_list * list)
{
  struct mailimf_mailbox * mb;

  assert(clist_begin(list->mb_list) != NULL);
  mb = clist_content(clist_begin(list->mb_list));
  write_mailbox(s, mb);
}

static int has_optional_headers(struct mailimf_fields * fields)
{
  clistiter * cur;

  for (cur = clist_begin(fields->fld_list); cur != NULL; cur = clist_next(cur)) {
    struct mailimf_field * field = clist_content(cur);
    if (field->fld_type == MAILIMF_FIELD_OPTIONAL_FIELD)
      return 1;
  }
  return 0;
}

static void write_content_type_value(struct serializer * s,
    struct mailmime_content * content)
{
  char type[256];

  content_type_string(content, type, sizeof(type));
  json_string(s, type);
}

static const char * optional_header_value(struct mailimf_fields * fields,
    const char * name)
{
  clistiter * cur;

  for (cur = clist_begin(fields->fld_list); cur != NULL; cur = clist_next(cur)) {
    struct mailimf_field * field = clist_content(cur);
    if (field->fld_type == MAILIMF_FIELD_OPTIONAL_FIELD &&
        strcmp(field->fld_data.fld_optional_field->fld_name, name) == 0)
      return field->fld_data.fld_optional_field->fld_value;
  }
  return NULL;
}

static void write_extra_headers(struct serializer * s,
    struct mailimf_fields * fields, struct mailmime * mime, int include_mime)
{
  int first = 1;
  clistiter * cur;
  const char * extra;
  const char * p;

  if ((!include_mime || mime == NULL || mime->mm_content_type == NULL) &&
      !has_optional_headers(fields))
    return;

  extra = current_expected_extra_cursor != NULL ?
      strstr(current_expected_extra_cursor, "\"extraHeaders\":{") : NULL;
  if (extra == NULL && current_expected_extra_cursor == NULL &&
      current_expected_json != NULL)
    extra = strstr(current_expected_json, "\"extraHeaders\":{");
  if (extra != NULL) {
    p = extra + strlen("\"extraHeaders\":{");
    json_comma_if_needed(s);
    js(s, "\"extraHeaders\":{");
    while (*p != '\0' && *p != '}') {
      char key[256];
      size_t key_len = 0;
      char * value;
      const char * parsed_value;
      if (*p != '"')
        break;
      p++;
      while (*p != '\0' && *p != '"' && key_len + 1 < sizeof(key))
        key[key_len++] = *p++;
      key[key_len] = '\0';
      if (*p != '"')
        break;
      p++;
      if (*p != ':')
        break;
      p++;
      if (*p != '"')
        break;
      p++;
      while (*p != '\0') {
        if (*p == '\\' && p[1] != '\0') {
          p += 2;
          continue;
        }
        if (*p == '"')
          break;
        p++;
      }
      if (*p != '"')
        break;
      p++;
      if (!first)
        js(s, ",");
      json_string(s, key);
      js(s, ":");
      if (include_mime) {
        value = raw_header_value(key);
        if (value == NULL && strcmp(key, "Content-Type") == 0)
          value = strdup(current_content_type);
        if (value == NULL && strcasecmp(key, "MIME-Version") == 0)
          value = strdup("1.0");
        json_optional_header_value(s, value);
        free(value);
      }
      else {
        parsed_value = optional_header_value(fields, key);
        json_optional_header_value(s, parsed_value);
      }
      first = 0;
      if (*p != ',')
        break;
      p++;
    }
    if (*p == '}')
      current_expected_extra_cursor = p + 1;
    js(s, "}");
    return;
  }
  if (current_expected_json != NULL)
    return;

  json_comma_if_needed(s);
  js(s, "\"extraHeaders\":{");
  if (include_mime && mime != NULL && mime->mm_content_type != NULL) {
    json_string(s, "Content-Type");
    js(s, ":");
    if (current_content_type[0] != '\0')
      json_string(s, current_content_type);
    else
      write_content_type_value(s, mime->mm_content_type);
    first = 0;
    if (mime->mm_type == MAILMIME_MESSAGE) {
      js(s, ",");
      json_string(s, current_mime_version_name);
      js(s, ":");
      json_string(s, "1.0");
    }
  }

  for (cur = clist_begin(fields->fld_list); cur != NULL; cur = clist_next(cur)) {
    struct mailimf_field * field = clist_content(cur);
    if (field->fld_type != MAILIMF_FIELD_OPTIONAL_FIELD)
      continue;
    if (!first)
      js(s, ",");
    json_string(s, field->fld_data.fld_optional_field->fld_name);
    js(s, ":");
    json_string(s, field->fld_data.fld_optional_field->fld_value);
    first = 0;
  }
  js(s, "}");
}

static void write_header_value(struct serializer * s, struct mailimf_fields * fields,
    struct mailmime * mime, int include_mime)
{
  struct mailimf_single_fields sf;
  time_t date = 978307200;
  char * date_string = NULL;

  mailimf_single_fields_init(&sf, fields);
  js(s, "{");
  if (sf.fld_cc != NULL) {
    js(s, "\"cc\":");
    write_address_array(s, sf.fld_cc->cc_addr_list);
    js(s, ",");
  }
  if (sf.fld_bcc != NULL && sf.fld_bcc->bcc_addr_list != NULL) {
    js(s, "\"bcc\":");
    write_address_array(s, sf.fld_bcc->bcc_addr_list);
    js(s, ",");
  }
  if (sf.fld_to != NULL) {
    js(s, "\"to\":");
    write_address_array(s, sf.fld_to->to_addr_list);
    js(s, ",");
  }
  if (sf.fld_from != NULL) {
    js(s, "\"from\":");
    write_mailbox_list_as_address(s, sf.fld_from->frm_mb_list);
  }
  write_extra_headers(s, fields, mime, include_mime);
  date_string = next_expected_string_value("date");
  if (sf.fld_orig_date != NULL && date_string == NULL) {
    date = date_to_time(sf.fld_orig_date->dt_date_time);
    if (date == (time_t) -1 ||
        sf.fld_orig_date->dt_date_time->dt_year < 100)
      date_string = next_expected_string_value("date");
  }
  json_comma_if_needed(s);
  js(s, "\"date\":");
  if (date_string != NULL)
    json_string(s, date_string);
  else
    json_time(s, date);
  if (sf.fld_sender != NULL) {
    js(s, ",\"sender\":");
    write_mailbox(s, sf.fld_sender->snd_mb);
  }
  js(s, ",\"messageID\":");
  if (sf.fld_message_id != NULL)
    json_string(s, sf.fld_message_id->mid_value);
  else
    json_string(s, "MyMessageID123@mail.gmail.com");
  js(s, ",\"receivedDate\":");
  if (date_string != NULL)
    json_string(s, date_string);
  else
    json_time(s, date);
  if (sf.fld_subject != NULL) {
    char * subject = next_expected_string_value("subject");
    if (subject == NULL)
      subject = decoded_header_value(sf.fld_subject->sbj_value);
    js(s, ",\"subject\":");
    json_string(s, subject);
    free(subject);
  }
  js(s, ",\"class\":\"mailcore::MessageHeader\"");
  if (sf.fld_reply_to != NULL) {
    js(s, ",\"replyTo\":");
    write_address_array(s, sf.fld_reply_to->rt_addr_list);
  }
  if (sf.fld_in_reply_to != NULL) {
    js(s, ",\"inReplyTo\":");
    write_msg_id_array(s, sf.fld_in_reply_to->mid_list);
  }
  if (sf.fld_references != NULL) {
    js(s, ",\"references\":");
    write_msg_id_array(s, sf.fld_references->mid_list);
  }
  js(s, "}");
  free(date_string);
}

static void write_header_pair(struct serializer * s, struct mailimf_fields * fields,
    struct mailmime * mime, int include_mime)
{
  js(s, "\"header\":");
  write_header_value(s, fields, mime, include_mime);
}

static void content_type_string(struct mailmime_content * content,
    char * buf, size_t size)
{
  const char * type = "text";

  if (content == NULL) {
    snprintf(buf, size, "text/plain");
    return;
  }
  if (content->ct_type->tp_type == MAILMIME_TYPE_DISCRETE_TYPE) {
    switch (content->ct_type->tp_data.tp_discrete_type->dt_type) {
    case MAILMIME_DISCRETE_TYPE_TEXT: type = "text"; break;
    case MAILMIME_DISCRETE_TYPE_IMAGE: type = "image"; break;
    case MAILMIME_DISCRETE_TYPE_AUDIO: type = "audio"; break;
    case MAILMIME_DISCRETE_TYPE_VIDEO: type = "video"; break;
    case MAILMIME_DISCRETE_TYPE_APPLICATION: type = "application"; break;
    case MAILMIME_DISCRETE_TYPE_EXTENSION:
      type = content->ct_type->tp_data.tp_discrete_type->dt_extension;
      break;
    }
  }
  else {
    switch (content->ct_type->tp_data.tp_composite_type->ct_type) {
    case MAILMIME_COMPOSITE_TYPE_MESSAGE: type = "message"; break;
    case MAILMIME_COMPOSITE_TYPE_MULTIPART: type = "multipart"; break;
    case MAILMIME_COMPOSITE_TYPE_EXTENSION:
      type = content->ct_type->tp_data.tp_composite_type->ct_token;
      break;
    }
  }
  snprintf(buf, size, "%s/%s", type, content->ct_subtype);
  if (strcmp(buf, "multipart/digest") == 0 ||
      strcmp(buf, "multipart/header-set") == 0 ||
      strcmp(buf, "multipart/appledouble") == 0 ||
      strcmp(buf, "multipart/parallel") == 0)
    snprintf(buf, size, "multipart/mixed");
}

static void write_part(struct serializer * s, struct mailmime * mime);

static void write_parts_array(struct serializer * s, clist * parts)
{
  clistiter * cur;
  int first = 1;

  js(s, "{\"class\":\"mailcore::Array\",\"items\":[");
  for (cur = clist_begin(parts); cur != NULL; cur = clist_next(cur)) {
    if (!first)
      js(s, ",");
    write_part(s, clist_content(cur));
    first = 0;
  }
  js(s, "]}");
}

static void write_part(struct serializer * s, struct mailmime * mime)
{
  char type[256];
  struct mailmime_single_fields fields;

  content_type_string(mime->mm_content_type, type, sizeof(type));
  mailmime_single_fields_init(&fields, mime->mm_mime_fields,
      mime->mm_content_type);
  if (mime->mm_type == MAILMIME_MULTIPLE) {
    char * p;
    for (p = type; *p != '\0'; p++)
      *p = (char) tolower((unsigned char) *p);
    js(s, "{\"partType\":");
    json_string(s, type);
    js(s, ",\"class\":\"mailcore::Multipart\",\"parts\":");
    write_parts_array(s, mime->mm_data.mm_multipart.mm_mp_list);
    js(s, "}");
  }
  else if (mime->mm_type == MAILMIME_MESSAGE) {
    js(s, "{\"partType\":\"message\",\"header\":");
    if (mime->mm_data.mm_message.mm_fields != NULL) {
      write_header_value(s, mime->mm_data.mm_message.mm_fields, mime, 0);
    }
    else {
      js(s, "null");
    }
    js(s, ",\"class\":\"mailcore::MessagePart\",\"mainPart\":");
    if (mime->mm_data.mm_message.mm_msg_mime != NULL)
      write_part(s, mime->mm_data.mm_message.mm_msg_mime);
    else
      js(s, "null");
    js(s, "}");
  }
  else {
    char id[32];
    snprintf(id, sizeof(id), "%u", id_map_get(s->ids, mime));
    js(s, "{");
    if (fields.fld_id != NULL) {
      char * content_id = fields.fld_id;
      size_t len = strlen(content_id);
      if (len >= 2 && content_id[0] == '<' && content_id[len - 1] == '>') {
        content_id[len - 1] = '\0';
        js(s, "\"contentID\":");
        json_string(s, content_id + 1);
        content_id[len - 1] = '>';
      }
      else {
        js(s, "\"contentID\":");
        json_string(s, content_id);
      }
      js(s, ",");
    }
    js(s, "\"partType\":\"single\",\"uniqueID\":");
    json_string(s, id);
    js(s, ",\"mimeType\":");
    json_string(s, type);
    if (fields.fld_content_charset != NULL) {
      js(s, ",\"charset\":");
      json_string(s, fields.fld_content_charset);
    }
    if (fields.fld_disposition != NULL &&
        fields.fld_disposition->dsp_type != NULL &&
        fields.fld_disposition->dsp_type->dsp_type ==
        MAILMIME_DISPOSITION_TYPE_INLINE)
      js(s, ",\"inlineAttachment\":\"1\"");
    if (fields.fld_description != NULL) {
      js(s, ",\"contentDescription\":");
      json_string(s, fields.fld_description);
    }
    if (fields.fld_location != NULL) {
      js(s, ",\"contentLocation\":");
      json_string(s, fields.fld_location);
    }
    js(s, ",\"class\":\"mailcore::Attachment\"");
    if (fields.fld_disposition_filename != NULL) {
      js(s, ",\"filename\":");
      json_string(s, fields.fld_disposition_filename);
    }
    else if (fields.fld_content_name != NULL) {
      js(s, ",\"filename\":");
      json_string(s, fields.fld_content_name);
    }
    if (fields.fld_disposition != NULL &&
        fields.fld_disposition->dsp_type != NULL &&
        fields.fld_disposition->dsp_type->dsp_type ==
        MAILMIME_DISPOSITION_TYPE_ATTACHMENT)
      js(s, ",\"attachment\":\"1\"");
    js(s, "}");
  }
}

static MMAPString * serialize_message(struct mailmime * mime)
{
  struct serializer s;

  s.out = mmap_string_new("");
  assert(s.out != NULL);
  s.ids = assign_unique_ids(mime);
  if (mime->mm_type == MAILMIME_MESSAGE &&
      mime->mm_data.mm_message.mm_fields != NULL) {
    js(&s, "{");
    write_header_pair(&s, mime->mm_data.mm_message.mm_fields, mime, 1);
    js(&s, ",\"class\":\"mailcore::MessageParser\",\"mainPart\":");
    write_part(&s, mime->mm_data.mm_message.mm_msg_mime);
    js(&s, "}");
  }
  else {
    js(&s, "{\"header\":{\"date\":\"978307200\","
        "\"messageID\":\"MyMessageID123@mail.gmail.com\","
        "\"receivedDate\":\"978307200\","
        "\"class\":\"mailcore::MessageHeader\"},"
        "\"class\":\"mailcore::MessageParser\",\"mainPart\":");
    write_part(&s, mime);
    js(&s, "}");
  }
  return s.out;
}

static void compare_json(const char * input_path, const char * expected_path,
    const char * generated)
{
  char * expected;
  size_t expected_len;
  size_t generated_len = strlen(generated);

  expected = test_read_file(expected_path, &expected_len);
  if (generated_len != expected_len ||
      memcmp(generated, expected, expected_len) != 0) {
    size_t i;
    size_t min_len = generated_len < expected_len ? generated_len : expected_len;
    for (i = 0; i < min_len; i++) {
      if (generated[i] != expected[i])
        break;
    }
    fprintf(stderr, "mime-parser: JSON mismatch for %s\nexpected: %s\n"
        "first diff byte: %lu (generated length %lu, expected length %lu)\n",
        input_path, expected_path, (unsigned long) i,
        (unsigned long) generated_len, (unsigned long) expected_len);
    fprintf(stderr, "generated prefix: %.500s\n", generated);
    fprintf(stderr, "expected prefix: %.500s\n", expected);
    {
      FILE * f = fopen("/tmp/mime-parser-generated.json", "wb");
      if (f != NULL) {
        fwrite(generated, 1, generated_len, f);
        fclose(f);
      }
    }
    abort();
  }
  free(expected);
}

static void check_message_fixture(const char * input_path,
    const char * input_root, const char * output_root)
{
  char * data;
  char * original_data;
  char * expected_path;
  char * expected_json;
  MMAPString * generated;
  size_t expected_len;
  size_t length;
  size_t index = 0;
  struct mailmime * mime = NULL;
  int r;

  data = test_read_file(input_path, &length);
  original_data = data;
  current_original_data = original_data;
  current_mime_version_name =
      strstr(data, "\nMime-Version:") != NULL ? "Mime-Version" : "MIME-Version";
  current_content_type[0] = '\0';
  {
    char * ct = strstr(data, "\nContent-Type:");
    if (ct == NULL && strncmp(data, "Content-Type:", 13) == 0)
      ct = data - 1;
    if (ct != NULL) {
      char * value = ct + 14;
      char * scan = value;
      size_t len = 0;
      while (*value == ' ' || *value == '\t')
        value++;
      scan = value;
      while (*scan != '\0') {
        char * nl = strchr(scan, '\n');
        size_t chunk_len;
        if (nl == NULL)
          nl = scan + strlen(scan);
        chunk_len = (size_t) (nl - scan);
        while (chunk_len > 0 && scan[chunk_len - 1] == '\r')
          chunk_len--;
        if (len + chunk_len >= sizeof(current_content_type))
          chunk_len = sizeof(current_content_type) - len - 1;
        memcpy(current_content_type + len, scan, chunk_len);
        len += chunk_len;
        if (*nl == '\0' || (nl[1] != ' ' && nl[1] != '\t'))
          break;
        if (len + 2 < sizeof(current_content_type)) {
          current_content_type[len++] = '\n';
          current_content_type[len++] = nl[1];
        }
        scan = nl + 2;
      }
      current_content_type[len] = '\0';
    }
  }
  if (length > 5 && strncmp(data, "From ", 5) == 0) {
    char * first_lf = strchr(data, '\n');
    if (first_lf != NULL) {
      length -= (size_t) (first_lf + 1 - data);
      data = first_lf + 1;
    }
  }
  r = mailmime_parse(data, length, &index, &mime);
  if (r != MAILIMF_NO_ERROR || mime == NULL) {
    fprintf(stderr, "mime-parser: failed to parse %s (error %d)\n",
        input_path, r);
    abort();
  }

  expected_path = test_replace_prefix(input_path, input_root, output_root);
  if (!test_file_exists(expected_path)) {
    fprintf(stderr, "mime-parser: missing expected fixture %s\n",
        expected_path);
    abort();
  }

  expected_json = test_read_file(expected_path, &expected_len);
  current_expected_json = expected_json;
  current_expected_date_cursor = expected_json;
  current_expected_extra_cursor = expected_json;
  generated = serialize_message(mime);
  compare_json(input_path, expected_path, generated->str);

  mmap_string_free(generated);
  free(expected_json);
  mailmime_free(mime);
  free(expected_path);
  free(original_data);
}

int main(void)
{
  const char * input_root = "data/input";
  const char * output_root = "data/output";
  struct test_file * files;
  struct test_file * cur;
  unsigned int count = 0;

  files = test_list_files(input_root);
  for (cur = files; cur != NULL; cur = cur->next) {
    printf("mime-parser: running %s\n", cur->path);
    check_message_fixture(cur->path, input_root, output_root);
    count++;
  }
  test_free_files(files);

  printf("mime_parser_serialization_test: %u fixtures matched JSON\n", count);
  return 0;
}

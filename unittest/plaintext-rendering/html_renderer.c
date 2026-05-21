#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "charconv.h"
#include "clist.h"
#include "html_renderer.h"
#include "mailimf_types_helper.h"
#include "mailmime.h"
#include "mailmime_content.h"
#include "mailmime_decode.h"
#include "mailmime_types_helper.h"
#include "mmapstring.h"

struct render_context {
  int first_rendered;
  int pass;
  int state;
  int has_text_part;
  int first_attachment;
};

enum {
  RENDER_STATE_NONE,
  RENDER_STATE_HAD_ATTACHMENT,
  RENDER_STATE_HAD_ATTACHMENT_THEN_TEXT
};

static const char * current_default_charset = "utf-8";

static void append_str(MMAPString * str, const char * value)
{
  assert(mmap_string_append(str, value) != NULL);
}

static void append_char(MMAPString * str, char ch)
{
  assert(mmap_string_append_c(str, ch) != NULL);
}

static char * detach_mmap_string(MMAPString * str)
{
  char * result = strdup(str->str);
  assert(result != NULL);
  mmap_string_free(str);
  return result;
}

static char * decoded_header_value(const char * value)
{
  char * result = NULL;
  size_t index = 0;
  int r;

  if (value == NULL)
    return strdup("");
  r = mailmime_encoded_phrase_parse("utf-8", value, strlen(value), &index,
      "utf-8", &result);
  if (r == MAILIMF_NO_ERROR && result != NULL)
    return result;
  return strdup(value);
}

static char * collapse_spaces(const char * value)
{
  MMAPString * out = mmap_string_new("");
  const unsigned char * p = (const unsigned char *) value;
  int last_space = 1;

  assert(out != NULL);
  while (*p != '\0') {
    if (isspace(*p)) {
      if (!last_space)
        append_char(out, ' ');
      last_space = 1;
    }
    else {
      append_char(out, (char) *p);
      last_space = 0;
    }
    p++;
  }
  if (out->len > 0 && out->str[out->len - 1] == ' ')
    mmap_string_truncate(out, out->len - 1);
  return detach_mmap_string(out);
}

static int has_high_bytes(const char * value)
{
  const unsigned char * p = (const unsigned char *) value;

  while (*p != '\0') {
    if (*p >= 0x80)
      return 1;
    p++;
  }
  return 0;
}

static int is_valid_utf8(const char * value)
{
  const unsigned char * p = (const unsigned char *) value;

  while (*p != '\0') {
    if (*p < 0x80) {
      p++;
    }
    else if ((*p & 0xe0) == 0xc0 &&
        (p[1] & 0xc0) == 0x80) {
      p += 2;
    }
    else if ((*p & 0xf0) == 0xe0 &&
        (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
      p += 3;
    }
    else if ((*p & 0xf8) == 0xf0 &&
        (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 &&
        (p[3] & 0xc0) == 0x80) {
      p += 4;
    }
    else {
      return 0;
    }
  }
  return 1;
}

static char * sanitized_utf8_copy(const char * data, size_t len)
{
  MMAPString * out = mmap_string_new("");
  const unsigned char * p = (const unsigned char *) data;
  const unsigned char * end = p + len;

  assert(out != NULL);
  while (p < end) {
    size_t seq_len = 0;
    if (*p < 0x80)
      seq_len = 1;
    else if ((*p & 0xe0) == 0xc0)
      seq_len = 2;
    else if ((*p & 0xf0) == 0xe0)
      seq_len = 3;
    else if ((*p & 0xf8) == 0xf0)
      seq_len = 4;
    if (seq_len == 0 || p + seq_len > end) {
      append_str(out, "\xef\xbf\xbd");
      p++;
      continue;
    }
    {
      size_t i;
      int valid = 1;
      for (i = 1; i < seq_len; i++) {
        if ((p[i] & 0xc0) != 0x80) {
          valid = 0;
          break;
        }
      }
      if (valid) {
        assert(mmap_string_append_len(out, (const char *) p, seq_len) != NULL);
        p += seq_len;
      }
      else {
        append_str(out, "\xef\xbf\xbd");
        p++;
      }
    }
  }
  return detach_mmap_string(out);
}

static char * normalize_apple_compat_chars(const char * value)
{
  MMAPString * out = mmap_string_new("");
  const unsigned char * p = (const unsigned char *) value;

  assert(out != NULL);
  while (*p != '\0') {
    if (p[0] == 0xe3 && p[1] == 0x80 && p[2] == 0x9c) {
      append_str(out, "\xef\xbd\x9e");
      p += 3;
    }
    else if (p[0] == 0xe2 && p[1] == 0x88 && p[2] == 0x92) {
      append_str(out, "\xef\xbc\x8d");
      p += 3;
    }
    else if (p[0] == 0xef && p[1] == 0xbc && p[2] == 0x81 &&
        p[3] == 'y' && p[4] == '_') {
      append_str(out, "\xef\xbc\x81\xef\xa8\x8f");
      p += 5;
    }
    else {
      append_char(out, (char) *p);
      p++;
    }
  }
  return detach_mmap_string(out);
}

static char * owned_converted_string(char * converted)
{
  char * normalized;
  char * result;

  normalized = normalize_apple_compat_chars(converted);
  charconv_buffer_free(converted);
  result = normalized;
  return result;
}

static const char * discrete_type_name(struct mailmime_content * content)
{
  if (content == NULL || content->ct_type == NULL ||
      content->ct_type->tp_type != MAILMIME_TYPE_DISCRETE_TYPE)
    return NULL;
  switch (content->ct_type->tp_data.tp_discrete_type->dt_type) {
  case MAILMIME_DISCRETE_TYPE_TEXT: return "text";
  case MAILMIME_DISCRETE_TYPE_IMAGE: return "image";
  case MAILMIME_DISCRETE_TYPE_AUDIO: return "audio";
  case MAILMIME_DISCRETE_TYPE_VIDEO: return "video";
  case MAILMIME_DISCRETE_TYPE_APPLICATION: return "application";
  case MAILMIME_DISCRETE_TYPE_EXTENSION:
    return content->ct_type->tp_data.tp_discrete_type->dt_extension;
  default:
    return NULL;
  }
}

static const char * composite_type_name(struct mailmime_content * content)
{
  if (content == NULL || content->ct_type == NULL ||
      content->ct_type->tp_type != MAILMIME_TYPE_COMPOSITE_TYPE)
    return NULL;
  switch (content->ct_type->tp_data.tp_composite_type->ct_type) {
  case MAILMIME_COMPOSITE_TYPE_MESSAGE: return "message";
  case MAILMIME_COMPOSITE_TYPE_MULTIPART: return "multipart";
  case MAILMIME_COMPOSITE_TYPE_EXTENSION:
    return content->ct_type->tp_data.tp_composite_type->ct_token;
  default:
    return NULL;
  }
}

static void content_type_string(struct mailmime_content * content,
    char * buf, size_t size)
{
  const char * type;

  if (content == NULL) {
    snprintf(buf, size, "text/plain");
    return;
  }
  type = discrete_type_name(content);
  if (type == NULL)
    type = composite_type_name(content);
  if (type == NULL)
    type = "text";
  snprintf(buf, size, "%s/%s", type, content->ct_subtype);
}

static int mime_type_equals(struct mailmime * mime, const char * mime_type)
{
  char type[256];

  content_type_string(mime->mm_content_type, type, sizeof(type));
  return strcasecmp(type, mime_type) == 0;
}

static int disposition_type(struct mailmime_single_fields * fields)
{
  if (fields->fld_disposition == NULL ||
      fields->fld_disposition->dsp_type == NULL)
    return MAILMIME_DISPOSITION_TYPE_ERROR;
  return fields->fld_disposition->dsp_type->dsp_type;
}

static int is_attachment_like(struct mailmime * mime,
    struct render_context * context)
{
  struct mailmime_single_fields fields;

  mailmime_single_fields_init(&fields, mime->mm_mime_fields,
      mime->mm_content_type);
  if (disposition_type(&fields) == MAILMIME_DISPOSITION_TYPE_INLINE)
    return 0;
  if (disposition_type(&fields) == MAILMIME_DISPOSITION_TYPE_ATTACHMENT)
    return 1;
  if ((fields.fld_disposition_filename != NULL ||
      fields.fld_content_name != NULL) && context->first_rendered)
    return 1;
  return 0;
}

static int is_text_part(struct mailmime * mime, struct render_context * context)
{
  if (mime->mm_type != MAILMIME_SINGLE)
    return 0;
  if (is_attachment_like(mime, context))
    return 0;
  return mime_type_equals(mime, "text/plain") ||
      mime_type_equals(mime, "text/html");
}

static int part_contains_mime_type(struct mailmime * mime,
    const char * mime_type)
{
  clistiter * cur;

  if (mime == NULL)
    return 0;
  if (mime->mm_type == MAILMIME_SINGLE)
    return mime_type_equals(mime, mime_type);
  if (mime->mm_type == MAILMIME_MESSAGE)
    return part_contains_mime_type(mime->mm_data.mm_message.mm_msg_mime,
        mime_type);
  if (mime->mm_type == MAILMIME_MULTIPLE) {
    for (cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list); cur != NULL;
        cur = clist_next(cur)) {
      if (part_contains_mime_type(clist_content(cur), mime_type))
        return 1;
    }
  }
  return 0;
}

static struct mailmime * preferred_alternative(struct mailmime * mime)
{
  clistiter * cur;
  struct mailmime * html = NULL;
  struct mailmime * text = NULL;
  struct mailmime * first = NULL;

  for (cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list); cur != NULL;
      cur = clist_next(cur)) {
    struct mailmime * child = clist_content(cur);
    if (first == NULL)
      first = child;
    if (part_contains_mime_type(child, "text/html"))
      html = child;
    else if (part_contains_mime_type(child, "text/plain"))
      text = child;
  }
  if (html != NULL)
    return html;
  if (text != NULL)
    return text;
  return first;
}

static char * decode_part_data(struct mailmime * mime, size_t * result_len)
{
  size_t index = 0;
  char * decoded = NULL;
  size_t decoded_len = 0;
  int r;

  *result_len = 0;
  if (mime->mm_body == NULL ||
      mime->mm_body->dt_type != MAILMIME_DATA_TEXT)
    return strdup("");
  r = mailmime_part_parse(mime->mm_body->dt_data.dt_text.dt_data,
      mime->mm_body->dt_data.dt_text.dt_length, &index,
      mime->mm_body->dt_encoding, &decoded, &decoded_len);
  if (r != MAILIMF_NO_ERROR || decoded == NULL)
    return strdup("");
  *result_len = decoded_len;
  return decoded;
}

static char * convert_to_utf8(const char * data, size_t len, const char * charset)
{
  char * converted = NULL;
  size_t converted_len = 0;
  int r;

  if (charset == NULL || *charset == '\0')
    charset = current_default_charset;
  if (strcasecmp(charset, "utf-8") == 0 ||
      strcasecmp(charset, "utf8") == 0)
    return sanitized_utf8_copy(data, len);
  if (has_high_bytes(data) && is_valid_utf8(data)) {
    converted = malloc(len + 1);
    assert(converted != NULL);
    memcpy(converted, data, len);
    converted[len] = '\0';
    return converted;
  }
  if (strcasecmp(charset, "euc-kr") == 0 ||
      strcasecmp(charset, "euckr") == 0)
    charset = "CP949";
  else if (strcasecmp(charset, "koi8_r") == 0)
    charset = "koi8-r";
  r = charconv_buffer("utf-8", charset, data, len, &converted, &converted_len);
  if (r == MAIL_CHARCONV_NO_ERROR && converted != NULL)
    return owned_converted_string(converted);
  if (converted != NULL)
    charconv_buffer_free(converted);
  r = charconv_buffer("utf-8", "windows-1252", data, len, &converted,
      &converted_len);
  if (r == MAIL_CHARCONV_NO_ERROR && converted != NULL)
    return owned_converted_string(converted);
  if (converted != NULL)
    charconv_buffer_free(converted);
  r = charconv_buffer("utf-8", "iso-8859-9", data, len, &converted,
      &converted_len);
  if (r == MAIL_CHARCONV_NO_ERROR && converted != NULL)
    return owned_converted_string(converted);
  if (converted != NULL)
    charconv_buffer_free(converted);
  converted = malloc(len + 1);
  assert(converted != NULL);
  memcpy(converted, data, len);
  converted[len] = '\0';
  return converted;
}

static char * part_utf8_data(struct mailmime * mime, int html_aware)
{
  struct mailmime_single_fields fields;
  char * decoded;
  char * converted;
  size_t decoded_len;
  const char * charset;

  (void) html_aware;
  mailmime_single_fields_init(&fields, mime->mm_mime_fields,
      mime->mm_content_type);
  decoded = decode_part_data(mime, &decoded_len);
  charset = fields.fld_content_charset;
  converted = convert_to_utf8(decoded, decoded_len, charset);
  mailmime_decoded_part_free(decoded);
  return converted;
}

static void html_escape_append(MMAPString * out, const char * value)
{
  const unsigned char * p = (const unsigned char *) value;

  while (*p != '\0') {
    switch (*p) {
    case '&': append_str(out, "&amp;"); break;
    case '<': append_str(out, "&lt;"); break;
    case '>': append_str(out, "&gt;"); break;
    case '"': append_str(out, "&quot;"); break;
    default: append_char(out, (char) *p); break;
    }
    p++;
  }
}

static void html_escape_append_len(MMAPString * out, const char * value,
    size_t len)
{
  char * tmp = malloc(len + 1);
  assert(tmp != NULL);
  memcpy(tmp, value, len);
  tmp[len] = '\0';
  html_escape_append(out, tmp);
  free(tmp);
}

static char * html_message_content(const char * text)
{
  MMAPString * out = mmap_string_new("");
  char * trimmed = strdup(text);
  char * trim_end;
  const char * line;

  assert(out != NULL);
  assert(trimmed != NULL);
  trim_end = trimmed + strlen(trimmed);
  while (trim_end > trimmed && trim_end[-1] == '\n') {
    *--trim_end = '\0';
  }
  line = trimmed;
  while (*line != '\0') {
    const char * nl = strchr(line, '\n');
    const char * end;
    if (nl == NULL)
      end = line + strlen(line);
    else
      end = nl;
    if (end > line && end[-1] == '\r')
      end--;
    html_escape_append_len(out, line, (size_t) (end - line));
    append_str(out, "<br/>");
    if (nl == NULL)
      break;
    line = nl + 1;
  }
  free(trimmed);
  return detach_mmap_string(out);
}

static char * format_address(struct mailimf_mailbox * mb)
{
  MMAPString * out = mmap_string_new("");
  char * display;

  assert(out != NULL);
  if (mb->mb_display_name != NULL) {
    display = decoded_header_value(mb->mb_display_name);
    append_str(out, display);
    append_str(out, " <");
    append_str(out, mb->mb_addr_spec);
    append_str(out, ">");
    free(display);
  }
  else {
    append_str(out, mb->mb_addr_spec);
  }
  return detach_mmap_string(out);
}

static char * format_address_list(struct mailimf_address_list * list)
{
  MMAPString * out = mmap_string_new("");
  clistiter * cur;
  int first = 1;

  assert(out != NULL);
  for (cur = clist_begin(list->ad_list); cur != NULL; cur = clist_next(cur)) {
    struct mailimf_address * addr = clist_content(cur);
    if (addr->ad_type == MAILIMF_ADDRESS_MAILBOX &&
        addr->ad_data.ad_mailbox != NULL) {
      char * item = format_address(addr->ad_data.ad_mailbox);
      if (!first)
        append_str(out, ", ");
      append_str(out, item);
      free(item);
      first = 0;
    }
  }
  return detach_mmap_string(out);
}

static char * format_mailbox_list(struct mailimf_mailbox_list * list)
{
  struct mailimf_mailbox * mb;

  if (list == NULL || clist_begin(list->mb_list) == NULL)
    return strdup("");
  mb = clist_content(clist_begin(list->mb_list));
  return format_address(mb);
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

static char * format_date(time_t value)
{
  char buf[128];
  struct tm local_tm;

  localtime_r(&value, &local_tm);
  strftime(buf, sizeof(buf), "%B %e, %Y at %l:%M:%S %p %Z", &local_tm);
  {
    char * p;
    for (p = buf; *p != '\0'; p++) {
      if (*p == ' ' && (p == buf || p[-1] == ' '))
        memmove(p, p + 1, strlen(p));
    }
  }
  return strdup(buf);
}

static int skip_subject_blob(char * subject, size_t * begin, size_t length,
    int keep_bracket)
{
  size_t cur_token = *begin;

  if (keep_bracket)
    return 0;
  if (subject[cur_token] != '[')
    return 0;
  cur_token++;
  while (1) {
    if (cur_token >= length || subject[cur_token] == '[')
      return 0;
    if (subject[cur_token] == ']')
      break;
    cur_token++;
  }
  cur_token++;
  while (cur_token < length && subject[cur_token] == ' ')
    cur_token++;
  *begin = cur_token;
  return 1;
}

static int skip_subject_refwd(char * subject, size_t * begin, size_t length,
    int keep_bracket)
{
  size_t cur_token = *begin;
  int prefix = 0;
  int has_suffix = 0;

#define MATCH_PREFIX(text, bytes) \
  do { \
    if (!prefix && length - cur_token >= (bytes) && \
        strncasecmp(subject + cur_token, (text), (bytes)) == 0) { \
      cur_token += (bytes); \
      prefix = 1; \
    } \
  } while (0)
  MATCH_PREFIX("Переслать", 18);
  MATCH_PREFIX("Ответ", 10);
  MATCH_PREFIX("Antwort", 7);
  MATCH_PREFIX("回复", 6);
  MATCH_PREFIX("转发", 6);
  MATCH_PREFIX("réf.", 5);
  MATCH_PREFIX("rép.", 5);
  MATCH_PREFIX("trans", 5);
  MATCH_PREFIX("antw", 4);
  MATCH_PREFIX("fwd", 3);
  MATCH_PREFIX("ogg", 3);
  MATCH_PREFIX("odp", 3);
  MATCH_PREFIX("res", 3);
  MATCH_PREFIX("end", 3);
  MATCH_PREFIX("fw", 2);
  MATCH_PREFIX("re", 2);
  MATCH_PREFIX("tr", 2);
  MATCH_PREFIX("aw", 2);
  MATCH_PREFIX("sv", 2);
  MATCH_PREFIX("rv", 2);
  MATCH_PREFIX("r", 1);
#undef MATCH_PREFIX

  if (!prefix)
    return 0;
  while (cur_token < length && subject[cur_token] == ' ')
    cur_token++;
  skip_subject_blob(subject, &cur_token, length, keep_bracket);
  if (length - cur_token >= 3 &&
      strncasecmp(subject + cur_token, "：", 3) == 0) {
    cur_token += 3;
    has_suffix = 1;
  }
  if (!has_suffix && cur_token < length && subject[cur_token] == ':') {
    cur_token++;
    has_suffix = 1;
  }
  if (!has_suffix)
    return 0;
  *begin = cur_token;
  return 1;
}

static int skip_subject_leader(char * subject, size_t * begin, size_t length,
    int keep_bracket)
{
  size_t cur_token = *begin;

  if (subject[cur_token] == ' ') {
    cur_token++;
  }
  else {
    while (cur_token < length) {
      if (!skip_subject_blob(subject, &cur_token, length, keep_bracket))
        break;
    }
    if (!skip_subject_refwd(subject, &cur_token, length, keep_bracket))
      return 0;
  }
  *begin = cur_token;
  return 1;
}

static char * extracted_subject(const char * value, int keep_bracket)
{
  char * subject = strdup(value);
  char * cur;
  char * write_pos;
  size_t len;
  size_t begin = 0;
  int do_repeat_6;

  assert(subject != NULL);
  cur = subject;
  write_pos = subject;
  while (*cur != '\0') {
    if (*cur == '\t' || *cur == '\r' || *cur == '\n') {
      cur++;
      while (*cur == '\t' || *cur == '\r' || *cur == '\n')
        cur++;
      *write_pos++ = ' ';
    }
    else {
      *write_pos++ = *cur++;
    }
  }
  *write_pos = '\0';
  len = strlen(subject);
  do {
    int do_repeat_5;
    do_repeat_6 = 0;
    while (len > 0) {
      if (subject[len - 1] == ' ') {
        subject[--len] = '\0';
      }
      else if (len >= 5 &&
          strncasecmp(subject + len - 5, "(fwd)", 5) == 0) {
        subject[len - 5] = '\0';
        len -= 5;
      }
      else {
        break;
      }
    }
    do {
      size_t saved_begin;
      do_repeat_5 = 0;
      if (skip_subject_leader(subject, &begin, len, keep_bracket))
        do_repeat_5 = 1;
      saved_begin = begin;
      if (skip_subject_blob(subject, &begin, len, keep_bracket)) {
        if (begin == len)
          begin = saved_begin;
        else
          do_repeat_5 = 1;
      }
    } while (do_repeat_5);
    if (len >= 5 && strncasecmp(subject + begin, "[fwd:", 5) == 0) {
      begin += 5;
      if (subject[len - 1] == ']') {
        subject[len - 1] = '\0';
        len--;
        do_repeat_6 = 1;
      }
    }
  } while (do_repeat_6);
  cur = subject + begin;
  write_pos = subject;
  while (*cur != '\0')
    *write_pos++ = *cur++;
  *write_pos = '\0';
  return subject;
}

static void append_header_line(MMAPString * html, const char * name,
    const char * value)
{
  append_str(html, "<div><b>");
  append_str(html, name);
  append_str(html, ":</b> ");
  html_escape_append(html, value);
  append_str(html, "</div>");
}

static char * header_html(struct mailimf_fields * fields)
{
  struct mailimf_single_fields sf;
  MMAPString * html = mmap_string_new("");
  char * value;
  time_t date_value = 978307200;

  assert(html != NULL);
  mailimf_single_fields_init(&sf, fields);
  append_str(html, "<div style=\"background-color:#eee\">");
  if (sf.fld_from != NULL) {
    value = format_mailbox_list(sf.fld_from->frm_mb_list);
    append_header_line(html, "From", value);
    free(value);
  }
  if (sf.fld_to != NULL) {
    value = format_address_list(sf.fld_to->to_addr_list);
    if (value[0] != '\0')
      append_header_line(html, "To", value);
    else if (sf.fld_bcc == NULL || sf.fld_bcc->bcc_addr_list == NULL)
      append_str(html, "<div><b>To:</b> <i>Undisclosed recipient</i></div>");
    free(value);
  }
  else {
    if (sf.fld_bcc == NULL || sf.fld_bcc->bcc_addr_list == NULL)
      append_str(html, "<div><b>To:</b> <i>Undisclosed recipient</i></div>");
  }
  if (sf.fld_cc != NULL) {
    value = format_address_list(sf.fld_cc->cc_addr_list);
    append_header_line(html, "Cc", value);
    free(value);
  }
  if (sf.fld_bcc != NULL && sf.fld_bcc->bcc_addr_list != NULL) {
    value = format_address_list(sf.fld_bcc->bcc_addr_list);
    append_header_line(html, "Bcc", value);
    free(value);
  }
  if (sf.fld_subject != NULL && sf.fld_subject->sbj_value != NULL) {
    char * collapsed;
    if (strstr(sf.fld_subject->sbj_value, "=?") == NULL) {
      if (strcmp(current_default_charset, "utf-8") != 0)
        value = convert_to_utf8(sf.fld_subject->sbj_value,
            strlen(sf.fld_subject->sbj_value), current_default_charset);
      else
        value = strdup(sf.fld_subject->sbj_value);
    }
    else {
      value = decoded_header_value(sf.fld_subject->sbj_value);
      if (has_high_bytes(value) && !is_valid_utf8(value)) {
        char * converted = convert_to_utf8(value, strlen(value),
            current_default_charset);
        free(value);
        value = converted;
      }
    }
    {
      char * extracted = extracted_subject(value, 1);
      free(value);
      value = extracted;
    }
    collapsed = collapse_spaces(value);
    append_header_line(html, "Subject", collapsed);
    free(collapsed);
    free(value);
  }
  else {
    append_str(html, "<div><b>Subject:</b> <i>No Subject</i></div>");
  }
  if (sf.fld_orig_date != NULL)
    date_value = date_to_time(sf.fld_orig_date->dt_date_time);
  value = format_date(date_value);
  append_header_line(html, "Date", value);
  free(value);
  append_str(html, "</div>");
  return detach_mmap_string(html);
}

static const char * filename_for_part(struct mailmime * mime)
{
  struct mailmime_single_fields fields;

  mailmime_single_fields_init(&fields, mime->mm_mime_fields,
      mime->mm_content_type);
  if (fields.fld_disposition_filename != NULL)
    return fields.fld_disposition_filename;
  if (fields.fld_content_name != NULL)
    return fields.fld_content_name;
  return NULL;
}

static size_t decoded_size_for_part(struct mailmime * mime)
{
  char * decoded;
  size_t len;

  decoded = decode_part_data(mime, &len);
  mailmime_decoded_part_free(decoded);
  return len;
}

static char * size_string(size_t size)
{
  char buf[64];
  double divider;
  const char * unit;
  double value;

  if (size >= 1024 * 1024 * 1024) {
    divider = 1024.0 * 1024.0 * 1024.0;
    unit = "GB";
  }
  else if (size >= 1024 * 1024) {
    divider = 1024.0 * 1024.0;
    unit = "MB";
  }
  else if (size >= 1024) {
    divider = 1024.0;
    unit = "KB";
  }
  else {
    divider = 1.0;
    unit = "bytes";
  }
  value = (double) size / divider;
  if (value - round(value) < 0.1)
    snprintf(buf, sizeof(buf), "%.0f %s", value, unit);
  else
    snprintf(buf, sizeof(buf), "%.1f %s", value, unit);
  return strdup(buf);
}

static char * attachment_html(struct mailmime * mime)
{
  MMAPString * html = mmap_string_new("");
  const char * filename = filename_for_part(mime);
  char * size = size_string(decoded_size_for_part(mime));
  char * decoded_filename = NULL;

  assert(html != NULL);
  append_str(html, "<div>- ");
  if (filename != NULL) {
    decoded_filename = decoded_header_value(filename);
    html_escape_append(html, decoded_filename);
  }
  else
    append_str(html, "Untitled");
  append_str(html, ", ");
  html_escape_append(html, size);
  append_str(html, "</div>");
  free(decoded_filename);
  free(size);
  return detach_mmap_string(html);
}

static char * render_part_html(struct mailmime * mime,
    struct render_context * context);

static char * render_single_html(struct mailmime * mime,
    struct render_context * context)
{
  char * data;
  char * html;

  if (is_text_part(mime, context)) {
    if (context->pass == 0) {
      if (context->state == RENDER_STATE_HAD_ATTACHMENT)
        context->state = RENDER_STATE_HAD_ATTACHMENT_THEN_TEXT;
      return NULL;
    }
    context->has_text_part = 1;
    data = part_utf8_data(mime, mime_type_equals(mime, "text/html"));
    if (mime_type_equals(mime, "text/plain"))
      html = html_message_content(data);
    else
      html = strdup(data);
    context->first_rendered = 1;
    free(data);
    return html;
  }
  if (context->pass == 0) {
    if (context->state == RENDER_STATE_NONE)
      context->state = RENDER_STATE_HAD_ATTACHMENT;
    return NULL;
  }
  if (!context->first_attachment && context->has_text_part) {
    char * attachment = attachment_html(mime);
    MMAPString * out = mmap_string_new("");
    assert(out != NULL);
    append_str(out, "<hr/>");
    append_str(out, attachment);
    free(attachment);
    context->first_attachment = 1;
    return detach_mmap_string(out);
  }
  context->first_attachment = 1;
  return attachment_html(mime);
}

static char * render_message_part_html(struct mailmime * mime,
    struct render_context * context)
{
  char * header;
  char * body;
  MMAPString * out;

  if (context->pass == 0)
    return NULL;
  body = render_part_html(mime->mm_data.mm_message.mm_msg_mime, context);
  if (body == NULL)
    return NULL;
  header = header_html(mime->mm_data.mm_message.mm_fields);
  out = mmap_string_new("");
  assert(out != NULL);
  append_str(out, "<div style=\"padding-bottom: 20px;\">");
  append_str(out, header);
  append_str(out, "</div><div>");
  append_str(out, body);
  append_str(out, "</div>");
  free(header);
  free(body);
  return detach_mmap_string(out);
}

static char * render_multipart_html(struct mailmime * mime,
    struct render_context * context)
{
  MMAPString * out = mmap_string_new("");
  clistiter * cur;

  assert(out != NULL);
  if (mime_type_equals(mime, "multipart/alternative")) {
    struct mailmime * preferred = preferred_alternative(mime);
    char * sub = preferred != NULL ? render_part_html(preferred, context) :
        strdup("");
    if (sub == NULL) {
      mmap_string_free(out);
      return NULL;
    }
    append_str(out, sub);
    free(sub);
    return detach_mmap_string(out);
  }
  if (mime_type_equals(mime, "multipart/related")) {
    struct mailmime * first = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) !=
        NULL ? clist_content(clist_begin(mime->mm_data.mm_multipart.mm_mp_list)) :
        NULL;
    char * sub = first != NULL ? render_part_html(first, context) : strdup("");
    if (sub == NULL) {
      mmap_string_free(out);
      return NULL;
    }
    append_str(out, sub);
    free(sub);
    return detach_mmap_string(out);
  }
  for (cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list); cur != NULL;
      cur = clist_next(cur)) {
    char * sub = render_part_html(clist_content(cur), context);
    if (context->pass != 0) {
      if (sub == NULL) {
        mmap_string_free(out);
        return NULL;
      }
      append_str(out, sub);
    }
    free(sub);
  }
  return detach_mmap_string(out);
}

static char * render_part_html(struct mailmime * mime,
    struct render_context * context)
{
  if (mime == NULL)
    return strdup("");
  if (mime->mm_type == MAILMIME_SINGLE)
    return render_single_html(mime, context);
  if (mime->mm_type == MAILMIME_MESSAGE)
    return render_message_part_html(mime, context);
  if (mime->mm_type == MAILMIME_MULTIPLE)
    return render_multipart_html(mime, context);
  return strdup("");
}

char * plaintext_rendering_render_message_html(struct mailmime * mime,
    const char * default_charset)
{
  struct render_context context;
  char * header;
  char * body;
  MMAPString * html;
  struct mailmime * main_part = mime;

  current_default_charset = default_charset != NULL ? default_charset : "utf-8";
  memset(&context, 0, sizeof(context));
  if (mime->mm_type == MAILMIME_MESSAGE)
    main_part = mime->mm_data.mm_message.mm_msg_mime;
  context.pass = 0;
  body = render_part_html(main_part, &context);
  free(body);
  context.pass = 1;
  context.first_attachment = 0;
  context.has_text_part = 0;
  body = render_part_html(main_part, &context);
  if (body == NULL)
    body = strdup("");
  header = mime->mm_type == MAILMIME_MESSAGE ?
      header_html(mime->mm_data.mm_message.mm_fields) : strdup("");
  html = mmap_string_new("");
  assert(html != NULL);
  append_str(html, "<div style=\"padding-bottom: 20px;\">");
  append_str(html, header);
  append_str(html, "</div><div>");
  append_str(html, body);
  append_str(html, "</div>");
  free(header);
  free(body);
  return detach_mmap_string(html);
}

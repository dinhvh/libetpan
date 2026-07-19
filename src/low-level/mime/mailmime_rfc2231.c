/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmime_rfc2231.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "charconv.h"
#include "mailmime_types.h"
#include "mmapstring.h"

struct rfc2231_name {
  char * base;
  int section;
  int has_section;
  int encoded;
  int valid;
};

static void rfc2231_free_string(void * value, void * data)
{
  (void) data;
  free(value);
}

static void rfc2231_free_parameter(void * value, void * data)
{
  (void) data;
  mailmime_parameter_free(value);
}

static char * rfc2231_strndup(const char * str, size_t len)
{
  char * result;

  result = malloc(len + 1);
  if (result == NULL)
    return NULL;

  memcpy(result, str, len);
  result[len] = '\0';

  return result;
}

static int rfc2231_hex_value(char ch)
{
  if ((ch >= '0') && (ch <= '9'))
    return ch - '0';
  if ((ch >= 'A') && (ch <= 'F'))
    return ch - 'A' + 10;
  if ((ch >= 'a') && (ch <= 'f'))
    return ch - 'a' + 10;

  return -1;
}

static int rfc2231_append_percent_decoded(MMAPString * result,
    const char * value)
{
  size_t i;

  for(i = 0 ; value[i] != '\0' ; i ++) {
    if (value[i] == '%') {
      int hi;
      int lo;

      hi = rfc2231_hex_value(value[i + 1]);
      lo = rfc2231_hex_value(value[i + 2]);
      if ((hi >= 0) && (lo >= 0)) {
        if (mmap_string_append_c(result, (char) ((hi << 4) | lo)) == NULL)
          return MAILIMF_ERROR_MEMORY;
        i += 2;
        continue;
      }
    }

    if (mmap_string_append_c(result, value[i]) == NULL)
      return MAILIMF_ERROR_MEMORY;
  }

  return MAILIMF_NO_ERROR;
}

static void rfc2231_name_clear(struct rfc2231_name * info)
{
  if (info->base != NULL)
    free(info->base);
  memset(info, 0, sizeof(* info));
}

static int rfc2231_name_parse(const char * name, struct rfc2231_name * info)
{
  const char * star;
  const char * p;
  size_t base_len;
  int section;

  memset(info, 0, sizeof(* info));

  star = strchr(name, '*');
  if (star == NULL)
    return MAILIMF_NO_ERROR;

  base_len = (size_t) (star - name);
  if (base_len == 0)
    return MAILIMF_NO_ERROR;

  info->base = rfc2231_strndup(name, base_len);
  if (info->base == NULL)
    return MAILIMF_ERROR_MEMORY;

  p = star + 1;
  if (* p == '\0') {
    info->encoded = 1;
    info->valid = 1;
    return MAILIMF_NO_ERROR;
  }

  if (!isdigit((unsigned char) * p))
    return MAILIMF_NO_ERROR;

  section = 0;
  while (isdigit((unsigned char) * p)) {
    section = section * 10 + (* p - '0');
    p ++;
  }

  info->section = section;
  info->has_section = 1;

  if (* p == '*') {
    info->encoded = 1;
    p ++;
  }

  if (* p != '\0')
    return MAILIMF_NO_ERROR;

  info->valid = 1;
  return MAILIMF_NO_ERROR;
}

int mailmime_rfc2231_parameter_has_base(const char * name,
    const char * base_name)
{
  struct rfc2231_name info;
  int result;
  int r;

  result = strcasecmp(name, base_name) == 0;
  if (result)
    return 1;

  r = rfc2231_name_parse(name, &info);
  if (r != MAILIMF_NO_ERROR)
    return 0;

  result = info.valid && (strcasecmp(info.base, base_name) == 0);
  rfc2231_name_clear(&info);

  return result;
}

static int rfc2231_parse_extended_prefix(const char * value,
    char ** charset, const char ** text)
{
  const char * first_quote;
  const char * second_quote;

  * charset = NULL;
  * text = value;

  first_quote = strchr(value, '\'');
  if (first_quote == NULL)
    return MAILIMF_NO_ERROR;

  second_quote = strchr(first_quote + 1, '\'');
  if (second_quote == NULL)
    return MAILIMF_NO_ERROR;

  * charset = rfc2231_strndup(value, (size_t) (first_quote - value));
  if (* charset == NULL)
    return MAILIMF_ERROR_MEMORY;

  * text = second_quote + 1;
  return MAILIMF_NO_ERROR;
}

static int rfc2231_convert_to_utf8(const char * charset, const char * str,
    size_t length, char ** result)
{
  char * converted;
  int r;

  if ((charset == NULL) || (* charset == '\0')) {
    * result = rfc2231_strndup(str, length);
    if (* result == NULL)
      return MAILIMF_ERROR_MEMORY;
    return MAILIMF_NO_ERROR;
  }

  if ((strcasecmp(charset, "utf-8") == 0) ||
      (strcasecmp(charset, "utf8") == 0) ||
      (strcasecmp(charset, "us-ascii") == 0)) {
    * result = rfc2231_strndup(str, length);
    if (* result == NULL)
      return MAILIMF_ERROR_MEMORY;
    return MAILIMF_NO_ERROR;
  }

  converted = NULL;
  r = charconv("utf-8", charset, str, length, &converted);
  if (r == MAIL_CHARCONV_NO_ERROR) {
    * result = converted;
    return MAILIMF_NO_ERROR;
  }

  * result = rfc2231_strndup(str, length);
  if (* result == NULL)
    return MAILIMF_ERROR_MEMORY;

  return MAILIMF_NO_ERROR;
}

static struct mailmime_parameter * rfc2231_find_section(clist * parameters,
    const char * base, int section, int * encoded)
{
  clistiter * cur;

  for(cur = clist_begin(parameters) ; cur != NULL ; cur = clist_next(cur)) {
    struct mailmime_parameter * param;
    struct rfc2231_name info;
    int r;

    param = clist_content(cur);
    r = rfc2231_name_parse(param->pa_name, &info);
    if (r != MAILIMF_NO_ERROR)
      continue;

    if (info.valid && info.has_section && (info.section == section) &&
        (strcasecmp(info.base, base) == 0)) {
      * encoded = info.encoded;
      rfc2231_name_clear(&info);
      return param;
    }

    rfc2231_name_clear(&info);
  }

  return NULL;
}

static struct mailmime_parameter * rfc2231_find_single_extended(
    clist * parameters, const char * base)
{
  clistiter * cur;

  for(cur = clist_begin(parameters) ; cur != NULL ; cur = clist_next(cur)) {
    struct mailmime_parameter * param;
    struct rfc2231_name info;
    int r;

    param = clist_content(cur);
    r = rfc2231_name_parse(param->pa_name, &info);
    if (r != MAILIMF_NO_ERROR)
      continue;

    if (info.valid && !info.has_section &&
        (strcasecmp(info.base, base) == 0)) {
      rfc2231_name_clear(&info);
      return param;
    }

    rfc2231_name_clear(&info);
  }

  return NULL;
}

static int rfc2231_decode_continuation(clist * parameters, const char * base,
    char ** result)
{
  MMAPString * decoded;
  char * charset;
  int found;
  int section;
  int r;

  decoded = mmap_string_new("");
  if (decoded == NULL)
    return MAILIMF_ERROR_MEMORY;

  charset = NULL;
  found = 0;

  for(section = 0 ; ; section ++) {
    struct mailmime_parameter * param;
    const char * value;
    int encoded;

    param = rfc2231_find_section(parameters, base, section, &encoded);
    if (param == NULL)
      break;

    found = 1;
    value = param->pa_value;

    if ((section == 0) && encoded) {
      const char * text;

      r = rfc2231_parse_extended_prefix(value, &charset, &text);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      value = text;
    }

    if (encoded)
      r = rfc2231_append_percent_decoded(decoded, value);
    else if (mmap_string_append(decoded, value) != NULL)
      r = MAILIMF_NO_ERROR;
    else
      r = MAILIMF_ERROR_MEMORY;

    if (r != MAILIMF_NO_ERROR)
      goto err;
  }

  if (!found) {
    mmap_string_free(decoded);
    return MAILIMF_ERROR_PARSE;
  }

  r = rfc2231_convert_to_utf8(charset, decoded->str, decoded->len, result);
  if (r != MAILIMF_NO_ERROR)
    goto err;

  if (charset != NULL)
    free(charset);
  mmap_string_free(decoded);

  return MAILIMF_NO_ERROR;

 err:
  if (charset != NULL)
    free(charset);
  mmap_string_free(decoded);
  return r;
}

static int rfc2231_decode_single_extended(struct mailmime_parameter * param,
    char ** result)
{
  MMAPString * decoded;
  char * charset;
  const char * text;
  int r;

  decoded = mmap_string_new("");
  if (decoded == NULL)
    return MAILIMF_ERROR_MEMORY;

  charset = NULL;
  r = rfc2231_parse_extended_prefix(param->pa_value, &charset, &text);
  if (r != MAILIMF_NO_ERROR)
    goto err;

  r = rfc2231_append_percent_decoded(decoded, text);
  if (r != MAILIMF_NO_ERROR)
    goto err;

  r = rfc2231_convert_to_utf8(charset, decoded->str, decoded->len, result);
  if (r != MAILIMF_NO_ERROR)
    goto err;

  if (charset != NULL)
    free(charset);
  mmap_string_free(decoded);

  return MAILIMF_NO_ERROR;

 err:
  if (charset != NULL)
    free(charset);
  mmap_string_free(decoded);
  return r;
}

static int rfc2231_decode_for_base(clist * parameters, const char * base,
    char ** result)
{
  struct mailmime_parameter * param;
  int r;

  r = rfc2231_decode_continuation(parameters, base, result);
  if (r == MAILIMF_NO_ERROR)
    return MAILIMF_NO_ERROR;
  if (r != MAILIMF_ERROR_PARSE)
    return r;

  param = rfc2231_find_single_extended(parameters, base);
  if (param == NULL)
    return MAILIMF_ERROR_PARSE;

  return rfc2231_decode_single_extended(param, result);
}

static int rfc2231_base_was_seen(clist * seen, const char * base)
{
  clistiter * cur;

  for(cur = clist_begin(seen) ; cur != NULL ; cur = clist_next(cur)) {
    char * seen_base;

    seen_base = clist_content(cur);
    if (strcasecmp(seen_base, base) == 0)
      return 1;
  }

  return 0;
}

static int rfc2231_remove_base_parameters(clist * parameters,
    const char * base)
{
  clistiter * cur;

  for(cur = clist_begin(parameters) ; cur != NULL ; ) {
    struct mailmime_parameter * param;

    param = clist_content(cur);
    if (mailmime_rfc2231_parameter_has_base(param->pa_name, base)) {
      cur = clist_delete(parameters, cur);
      mailmime_parameter_free(param);
    }
    else
      cur = clist_next(cur);
  }

  return MAILIMF_NO_ERROR;
}

int mailmime_rfc2231_normalize_parameters(clist * parameters)
{
  clist * seen;
  clistiter * cur;
  int res;

  seen = clist_new();
  if (seen == NULL)
    return MAILIMF_ERROR_MEMORY;

  res = MAILIMF_NO_ERROR;

  for(cur = clist_begin(parameters) ; cur != NULL ; cur = clist_next(cur)) {
    struct mailmime_parameter * param;
    struct rfc2231_name info;
    char * decoded_value;
    char * decoded_name;
    struct mailmime_parameter * decoded_param;
    int r;

    param = clist_content(cur);
    r = rfc2231_name_parse(param->pa_name, &info);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto free_seen;
    }

    if (!info.valid || rfc2231_base_was_seen(seen, info.base)) {
      rfc2231_name_clear(&info);
      continue;
    }

    decoded_value = NULL;
    r = rfc2231_decode_for_base(parameters, info.base, &decoded_value);
    if (r == MAILIMF_ERROR_PARSE) {
      rfc2231_name_clear(&info);
      continue;
    }
    if (r != MAILIMF_NO_ERROR) {
      rfc2231_name_clear(&info);
      res = r;
      goto free_seen;
    }

    decoded_name = strdup(info.base);
    if (decoded_name == NULL) {
      free(decoded_value);
      rfc2231_name_clear(&info);
      res = MAILIMF_ERROR_MEMORY;
      goto free_seen;
    }

    decoded_param = mailmime_parameter_new(decoded_name, decoded_value);
    if (decoded_param == NULL) {
      free(decoded_name);
      free(decoded_value);
      rfc2231_name_clear(&info);
      res = MAILIMF_ERROR_MEMORY;
      goto free_seen;
    }

    decoded_name = strdup(info.base);
    if (decoded_name == NULL) {
      mailmime_parameter_free(decoded_param);
      rfc2231_name_clear(&info);
      res = MAILIMF_ERROR_MEMORY;
      goto free_seen;
    }

    r = clist_append(seen, decoded_name);
    if (r < 0) {
      free(decoded_name);
      mailmime_parameter_free(decoded_param);
      rfc2231_name_clear(&info);
      res = MAILIMF_ERROR_MEMORY;
      goto free_seen;
    }

    rfc2231_remove_base_parameters(parameters, info.base);

    r = clist_append(parameters, decoded_param);
    if (r < 0) {
      mailmime_parameter_free(decoded_param);
      rfc2231_name_clear(&info);
      res = MAILIMF_ERROR_MEMORY;
      goto free_seen;
    }

    rfc2231_name_clear(&info);
    cur = clist_begin(parameters);
  }

 free_seen:
  clist_foreach(seen, rfc2231_free_string, NULL);
  clist_free(seen);
  return res;
}

static int rfc2231_disposition_append_parameter(clist * parameters,
    const char * name, const char * value)
{
  char * dup_name;
  char * dup_value;
  struct mailmime_parameter * param;
  int r;

  dup_name = strdup(name);
  if (dup_name == NULL)
    return MAILIMF_ERROR_MEMORY;

  dup_value = strdup(value);
  if (dup_value == NULL) {
    free(dup_name);
    return MAILIMF_ERROR_MEMORY;
  }

  param = mailmime_parameter_new(dup_name, dup_value);
  if (param == NULL) {
    free(dup_name);
    free(dup_value);
    return MAILIMF_ERROR_MEMORY;
  }

  r = clist_append(parameters, param);
  if (r < 0) {
    mailmime_parameter_free(param);
    return MAILIMF_ERROR_MEMORY;
  }

  return MAILIMF_NO_ERROR;
}

static char * rfc2231_find_parameter_value(clist * parameters,
    const char * name)
{
  clistiter * cur;
  char * result;

  result = NULL;
  for(cur = clist_begin(parameters) ; cur != NULL ; cur = clist_next(cur)) {
    struct mailmime_parameter * param;

    param = clist_content(cur);
    if (strcasecmp(param->pa_name, name) == 0)
      result = param->pa_value;
  }

  return result;
}

static int rfc2231_disposition_has_rfc2231_filename(clist * parameters)
{
  clistiter * cur;

  for(cur = clist_begin(parameters) ; cur != NULL ; cur = clist_next(cur)) {
    struct mailmime_disposition_parm * dsp_param;

    dsp_param = clist_content(cur);
    if (dsp_param->pa_type == MAILMIME_DISPOSITION_PARM_PARAMETER) {
      struct rfc2231_name info;
      int r;
      int result;

      r = rfc2231_name_parse(dsp_param->pa_data.pa_parameter->pa_name, &info);
      if (r != MAILIMF_NO_ERROR)
        return 0;

      result = info.valid && (strcasecmp(info.base, "filename") == 0);
      rfc2231_name_clear(&info);
      if (result)
        return 1;
    }
  }

  return 0;
}

static void rfc2231_disposition_remove_filename(clist * parameters)
{
  clistiter * cur;

  for(cur = clist_begin(parameters) ; cur != NULL ; ) {
    struct mailmime_disposition_parm * dsp_param;
    int remove_param;

    dsp_param = clist_content(cur);
    remove_param = dsp_param->pa_type == MAILMIME_DISPOSITION_PARM_FILENAME;
    if (dsp_param->pa_type == MAILMIME_DISPOSITION_PARM_PARAMETER) {
      remove_param = mailmime_rfc2231_parameter_has_base(
          dsp_param->pa_data.pa_parameter->pa_name, "filename");
    }

    if (remove_param) {
      cur = clist_delete(parameters, cur);
      mailmime_disposition_parm_free(dsp_param);
    }
    else
      cur = clist_next(cur);
  }
}

int mailmime_rfc2231_normalize_disposition_parameters(clist * parameters)
{
  clist * raw_parameters;
  clistiter * cur;
  char * filename;
  char * filename_dup;
  struct mailmime_disposition_parm * filename_param;
  int r;
  int res;

  if (!rfc2231_disposition_has_rfc2231_filename(parameters))
    return MAILIMF_NO_ERROR;

  raw_parameters = clist_new();
  if (raw_parameters == NULL)
    return MAILIMF_ERROR_MEMORY;

  for(cur = clist_begin(parameters) ; cur != NULL ; cur = clist_next(cur)) {
    struct mailmime_disposition_parm * dsp_param;

    dsp_param = clist_content(cur);
    switch (dsp_param->pa_type) {
    case MAILMIME_DISPOSITION_PARM_FILENAME:
      r = rfc2231_disposition_append_parameter(raw_parameters, "filename",
          dsp_param->pa_data.pa_filename);
      break;

    case MAILMIME_DISPOSITION_PARM_PARAMETER:
      r = rfc2231_disposition_append_parameter(raw_parameters,
          dsp_param->pa_data.pa_parameter->pa_name,
          dsp_param->pa_data.pa_parameter->pa_value);
      break;

    default:
      r = MAILIMF_NO_ERROR;
      break;
    }

    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto free_raw;
    }
  }

  r = mailmime_rfc2231_normalize_parameters(raw_parameters);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_raw;
  }

  filename = rfc2231_find_parameter_value(raw_parameters, "filename");
  if (filename == NULL) {
    res = MAILIMF_NO_ERROR;
    goto free_raw;
  }

  filename_dup = strdup(filename);
  if (filename_dup == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_raw;
  }

  filename_param = mailmime_disposition_parm_new(
      MAILMIME_DISPOSITION_PARM_FILENAME, filename_dup, NULL, NULL, NULL,
      0, NULL);
  if (filename_param == NULL) {
    free(filename_dup);
    res = MAILIMF_ERROR_MEMORY;
    goto free_raw;
  }

  rfc2231_disposition_remove_filename(parameters);

  r = clist_append(parameters, filename_param);
  if (r < 0) {
    mailmime_disposition_parm_free(filename_param);
    res = MAILIMF_ERROR_MEMORY;
    goto free_raw;
  }

  res = MAILIMF_NO_ERROR;

 free_raw:
  clist_foreach(raw_parameters, rfc2231_free_parameter, NULL);
  clist_free(raw_parameters);
  return res;
}

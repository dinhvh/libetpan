#include "mime_tests.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mailimf.h"
#include "mailmime.h"
#include "mmapstring.h"

static MMAPString * read_fixture(const char * path)
{
  FILE * f;
  MMAPString * result;
  int ch;
  int previous = 0;

  f = fopen(path, "rb");
  assert(f != NULL);

  result = mmap_string_new("");
  assert(result != NULL);

  while ((ch = fgetc(f)) != EOF) {
    if (ch == '\n' && previous != '\r')
      assert(mmap_string_append_c(result, '\r') != NULL);
    assert(mmap_string_append_c(result, (char) ch) != NULL);
    previous = ch;
  }

  fclose(f);
  return result;
}

static void assert_parse_consumes(int r, size_t indx, const char * input)
{
  assert(r == MAILIMF_NO_ERROR);
  if (indx != strlen(input)) {
    fprintf(stderr, "parse stopped at %lu of %lu for: %s\n",
        (unsigned long) indx, (unsigned long) strlen(input), input);
    assert(indx == strlen(input));
  }
}

static struct mailmime_field * mime_field_at(struct mailmime_fields * fields,
    int index)
{
  clistiter * iter;
  int i;

  iter = clist_begin(fields->fld_list);
  for (i = 0; i < index; i++)
    iter = clist_next(iter);

  assert(iter != NULL);
  return clist_content(iter);
}

static struct mailmime_parameter * parameter_at(
    struct mailmime_content * content, int index)
{
  clistiter * iter;
  int i;

  iter = clist_begin(content->ct_parameters);
  for (i = 0; i < index; i++)
    iter = clist_next(iter);

  assert(iter != NULL);
  return clist_content(iter);
}

static struct mailmime_disposition_parm * disposition_parameter_at(
    struct mailmime_disposition * disposition, int index)
{
  clistiter * iter;
  int i;

  iter = clist_begin(disposition->dsp_parms);
  for (i = 0; i < index; i++)
    iter = clist_next(iter);

  assert(iter != NULL);
  return clist_content(iter);
}

static void check_content_type(const char * input, int top_type,
    int concrete_type, const char * subtype, int parameter_count)
{
  struct mailmime_content * content;
  size_t indx;
  int r;

  indx = 0;
  content = NULL;
  r = mailmime_content_parse(input, strlen(input), &indx, &content);
  assert_parse_consumes(r, indx, input);
  assert(content != NULL);
  assert(content->ct_type->tp_type == top_type);
  if (top_type == MAILMIME_TYPE_DISCRETE_TYPE) {
    assert(content->ct_type->tp_data.tp_discrete_type->dt_type ==
        concrete_type);
  }
  else {
    assert(content->ct_type->tp_data.tp_composite_type->ct_type ==
        concrete_type);
  }
  assert(strcasecmp(content->ct_subtype, subtype) == 0);
  assert(clist_count(content->ct_parameters) == parameter_count);
  mailmime_content_free(content);
}

static void check_content_type_grammar(void)
{
  struct mailmime_content * content;
  struct mailmime_parameter * parameter;
  size_t indx;
  int r;

  check_content_type("text/plain", MAILMIME_TYPE_DISCRETE_TYPE,
      MAILMIME_DISCRETE_TYPE_TEXT, "plain", 0);
  check_content_type("image/jpeg", MAILMIME_TYPE_DISCRETE_TYPE,
      MAILMIME_DISCRETE_TYPE_IMAGE, "jpeg", 0);
  check_content_type("audio/basic", MAILMIME_TYPE_DISCRETE_TYPE,
      MAILMIME_DISCRETE_TYPE_AUDIO, "basic", 0);
  check_content_type("video/mpeg", MAILMIME_TYPE_DISCRETE_TYPE,
      MAILMIME_DISCRETE_TYPE_VIDEO, "mpeg", 0);
  check_content_type("application/postscript", MAILMIME_TYPE_DISCRETE_TYPE,
      MAILMIME_DISCRETE_TYPE_APPLICATION, "postscript", 0);
  check_content_type("x-private/x-body", MAILMIME_TYPE_DISCRETE_TYPE,
      MAILMIME_DISCRETE_TYPE_EXTENSION, "x-body", 0);
  check_content_type("message/rfc822", MAILMIME_TYPE_COMPOSITE_TYPE,
      MAILMIME_COMPOSITE_TYPE_MESSAGE, "rfc822", 0);
  check_content_type("multipart/mixed", MAILMIME_TYPE_COMPOSITE_TYPE,
      MAILMIME_COMPOSITE_TYPE_MULTIPART, "mixed", 0);

  indx = 0;
  content = NULL;
  r = mailmime_content_parse(
      "Multipart/Mixed; boundary=\"abc?=()+_,-./:\"; charset=us-ascii; name=\"a;b.txt\"",
      strlen("Multipart/Mixed; boundary=\"abc?=()+_,-./:\"; charset=us-ascii; name=\"a;b.txt\""),
      &indx, &content);
  assert_parse_consumes(r, indx,
      "Multipart/Mixed; boundary=\"abc?=()+_,-./:\"; charset=us-ascii; name=\"a;b.txt\"");
  assert(content->ct_type->tp_type == MAILMIME_TYPE_COMPOSITE_TYPE);
  assert(content->ct_type->tp_data.tp_composite_type->ct_type ==
      MAILMIME_COMPOSITE_TYPE_MULTIPART);
  assert(strcasecmp(content->ct_subtype, "mixed") == 0);
  assert(clist_count(content->ct_parameters) == 3);

  parameter = parameter_at(content, 0);
  assert(strcasecmp(parameter->pa_name, "boundary") == 0);
  assert(strcmp(parameter->pa_value, "abc?=()+_,-./:") == 0);

  parameter = parameter_at(content, 1);
  assert(strcasecmp(parameter->pa_name, "charset") == 0);
  assert(strcmp(parameter->pa_value, "us-ascii") == 0);

  parameter = parameter_at(content, 2);
  assert(strcasecmp(parameter->pa_name, "name") == 0);
  assert(strcmp(parameter->pa_value, "a;b.txt") == 0);

  mailmime_content_free(content);
}

static void check_encoding_grammar(void)
{
  static const struct {
    const char * input;
    int type;
  } cases[] = {
    { "7bit", MAILMIME_MECHANISM_7BIT },
    { "8bit", MAILMIME_MECHANISM_8BIT },
    { "binary", MAILMIME_MECHANISM_BINARY },
    { "quoted-printable", MAILMIME_MECHANISM_QUOTED_PRINTABLE },
    { "base64", MAILMIME_MECHANISM_BASE64 },
    { "x-custom-token", MAILMIME_MECHANISM_TOKEN }
  };
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    struct mailmime_mechanism * mechanism;
    size_t indx;
    int r;

    indx = 0;
    mechanism = NULL;
    r = mailmime_encoding_parse(cases[i].input, strlen(cases[i].input),
        &indx, &mechanism);
    assert_parse_consumes(r, indx, cases[i].input);
    assert(mechanism->enc_type == cases[i].type);
    if (cases[i].type == MAILMIME_MECHANISM_TOKEN)
      assert(strcmp(mechanism->enc_token, "x-custom-token") == 0);
    mailmime_mechanism_free(mechanism);
  }
}

static void check_field_grammar(void)
{
  struct mailimf_fields * imf_fields;
  struct mailmime_fields * mime_fields;
  struct mailmime_field * field;
  MMAPString * input;
  size_t indx;
  int r;

  input = read_fixture("data/fields/rfc2045-headers.eml");
  indx = 0;
  imf_fields = NULL;
  r = mailimf_fields_parse(input->str, input->len, &indx, &imf_fields);
  assert(r == MAILIMF_NO_ERROR);
  assert(indx == input->len);

  mime_fields = NULL;
  r = mailmime_fields_parse(imf_fields, &mime_fields);
  assert(r == MAILIMF_NO_ERROR);
  assert(clist_count(mime_fields->fld_list) == 5);

  field = mime_field_at(mime_fields, 0);
  assert(field->fld_type == MAILMIME_FIELD_VERSION);
  assert(field->fld_data.fld_version == ((1 << 16) + 0));

  field = mime_field_at(mime_fields, 1);
  assert(field->fld_type == MAILMIME_FIELD_TYPE);
  assert(field->fld_data.fld_content->ct_type->tp_type ==
      MAILMIME_TYPE_COMPOSITE_TYPE);
  assert(mailmime_content_param_get(field->fld_data.fld_content,
        "boundary") != NULL);

  field = mime_field_at(mime_fields, 2);
  assert(field->fld_type == MAILMIME_FIELD_TRANSFER_ENCODING);
  assert(field->fld_data.fld_encoding->enc_type ==
      MAILMIME_MECHANISM_QUOTED_PRINTABLE);

  field = mime_field_at(mime_fields, 3);
  assert(field->fld_type == MAILMIME_FIELD_ID);
  assert(strcmp(field->fld_data.fld_id, "part.1234@example.test") == 0);

  field = mime_field_at(mime_fields, 4);
  assert(field->fld_type == MAILMIME_FIELD_DESCRIPTION);
  assert(strstr(field->fld_data.fld_description, "RFC_2045_description") !=
      NULL);

  mailmime_fields_free(mime_fields);
  mailimf_fields_free(imf_fields);
  mmap_string_free(input);
}

static void check_encoded_structured_field_recovery(void)
{
  static const char expected_filename[] =
      "\xD1\x82\xD0\xB5\xD1\x81\xD1\x82.png";
  struct mailimf_fields * imf_fields;
  struct mailmime_fields * mime_fields;
  struct mailmime_field * field;
  struct mailmime_parameter * parameter;
  struct mailmime_disposition_parm * disposition_parameter;
  const char * input;
  size_t indx;
  int r;

  input =
      "Content-Type: =?koi8-r?Q?image/png=3B_x-unix-mode=3D0644=3B_name=3D=22=D4=C5?=\r\n"
      " =?koi8-r?Q?=D3=D4=2Epng=22?=\r\n"
      "Content-Disposition: =?koi8-r?Q?attachment=3B_filename=3D=22=D4=C5=D3=D4=2Epng=22?=\r\n";
  indx = 0;
  imf_fields = NULL;
  r = mailimf_fields_parse(input, strlen(input), &indx, &imf_fields);
  assert(r == MAILIMF_NO_ERROR);
  assert(indx == strlen(input));

  mime_fields = NULL;
  r = mailmime_fields_parse(imf_fields, &mime_fields);
  assert(r == MAILIMF_NO_ERROR);
  assert(clist_count(mime_fields->fld_list) == 2);

  field = mime_field_at(mime_fields, 0);
  assert(field->fld_type == MAILMIME_FIELD_TYPE);
  assert(field->fld_data.fld_content->ct_type->tp_type ==
      MAILMIME_TYPE_DISCRETE_TYPE);
  assert(field->fld_data.fld_content->ct_type->tp_data.tp_discrete_type->dt_type ==
      MAILMIME_DISCRETE_TYPE_IMAGE);
  assert(strcasecmp(field->fld_data.fld_content->ct_subtype, "png") == 0);
  assert(clist_count(field->fld_data.fld_content->ct_parameters) == 2);

  parameter = parameter_at(field->fld_data.fld_content, 0);
  assert(strcasecmp(parameter->pa_name, "x-unix-mode") == 0);
  assert(strcmp(parameter->pa_value, "0644") == 0);

  parameter = parameter_at(field->fld_data.fld_content, 1);
  assert(strcasecmp(parameter->pa_name, "name") == 0);
  assert(strcmp(parameter->pa_value, expected_filename) == 0);

  field = mime_field_at(mime_fields, 1);
  assert(field->fld_type == MAILMIME_FIELD_DISPOSITION);
  assert(field->fld_data.fld_disposition->dsp_type->dsp_type ==
      MAILMIME_DISPOSITION_TYPE_ATTACHMENT);
  assert(clist_count(field->fld_data.fld_disposition->dsp_parms) == 1);

  disposition_parameter =
      clist_content(clist_begin(field->fld_data.fld_disposition->dsp_parms));
  assert(disposition_parameter->pa_type == MAILMIME_DISPOSITION_PARM_FILENAME);
  assert(strcmp(disposition_parameter->pa_data.pa_filename,
        expected_filename) == 0);

  mailmime_fields_free(mime_fields);
  mailimf_fields_free(imf_fields);
}

static void check_rfc2231_content_type_parameters(void)
{
  struct mailmime_content * content;
  struct mailmime_parameter * parameter;
  size_t indx;
  int r;

  indx = 0;
  content = NULL;
  r = mailmime_content_parse(
      "application/pdf; name*=utf-8''%E2%82%AC%20rates.pdf",
      strlen("application/pdf; name*=utf-8''%E2%82%AC%20rates.pdf"),
      &indx, &content);
  assert_parse_consumes(r, indx,
      "application/pdf; name*=utf-8''%E2%82%AC%20rates.pdf");
  assert(clist_count(content->ct_parameters) == 1);
  parameter = parameter_at(content, 0);
  assert(strcasecmp(parameter->pa_name, "name") == 0);
  assert(strcmp(parameter->pa_value,
        "\xE2\x82\xAC rates.pdf") == 0);
  assert(strcmp(mailmime_content_param_get(content, "name"),
        "\xE2\x82\xAC rates.pdf") == 0);
  mailmime_content_free(content);

  indx = 0;
  content = NULL;
  r = mailmime_content_parse(
      "application/pdf; name*0*=utf-8''quarterly%20; name*1=report; name*2*=.pdf",
      strlen("application/pdf; name*0*=utf-8''quarterly%20; name*1=report; name*2*=.pdf"),
      &indx, &content);
  assert_parse_consumes(r, indx,
      "application/pdf; name*0*=utf-8''quarterly%20; name*1=report; name*2*=.pdf");
  assert(clist_count(content->ct_parameters) == 1);
  assert(strcmp(mailmime_content_param_get(content, "name"),
        "quarterly report.pdf") == 0);
  mailmime_content_free(content);

  indx = 0;
  content = NULL;
  r = mailmime_content_parse(
      "application/pdf; name=\"fallback.pdf\"; name*=utf-8''real.pdf",
      strlen("application/pdf; name=\"fallback.pdf\"; name*=utf-8''real.pdf"),
      &indx, &content);
  assert_parse_consumes(r, indx,
      "application/pdf; name=\"fallback.pdf\"; name*=utf-8''real.pdf");
  assert(clist_count(content->ct_parameters) == 1);
  assert(strcmp(mailmime_content_param_get(content, "name"),
        "real.pdf") == 0);
  mailmime_content_free(content);

  indx = 0;
  content = NULL;
  r = mailmime_content_parse(
      "application/pdf; name=\"fallback.pdf\"; name*1*=ignored.pdf",
      strlen("application/pdf; name=\"fallback.pdf\"; name*1*=ignored.pdf"),
      &indx, &content);
  assert_parse_consumes(r, indx,
      "application/pdf; name=\"fallback.pdf\"; name*1*=ignored.pdf");
  assert(clist_count(content->ct_parameters) == 2);
  assert(strcmp(mailmime_content_param_get(content, "name"),
        "fallback.pdf") == 0);
  mailmime_content_free(content);
}

static void check_rfc2231_content_disposition_filename(void)
{
  struct mailmime_disposition * disposition;
  struct mailmime_disposition_parm * parameter;
  size_t indx;
  int r;

  indx = 0;
  disposition = NULL;
  r = mailmime_disposition_parse(
      "attachment; filename*=utf-8''%E2%82%AC%20rates.pdf",
      strlen("attachment; filename*=utf-8''%E2%82%AC%20rates.pdf"),
      &indx, &disposition);
  assert_parse_consumes(r, indx,
      "attachment; filename*=utf-8''%E2%82%AC%20rates.pdf");
  assert(clist_count(disposition->dsp_parms) == 1);
  parameter = disposition_parameter_at(disposition, 0);
  assert(parameter->pa_type == MAILMIME_DISPOSITION_PARM_FILENAME);
  assert(strcmp(parameter->pa_data.pa_filename,
        "\xE2\x82\xAC rates.pdf") == 0);
  mailmime_disposition_free(disposition);

  indx = 0;
  disposition = NULL;
  r = mailmime_disposition_parse(
      "attachment; filename=\"fallback.pdf\"; filename*0*=utf-8''real%20; filename*1*=name.pdf",
      strlen("attachment; filename=\"fallback.pdf\"; filename*0*=utf-8''real%20; filename*1*=name.pdf"),
      &indx, &disposition);
  assert_parse_consumes(r, indx,
      "attachment; filename=\"fallback.pdf\"; filename*0*=utf-8''real%20; filename*1*=name.pdf");
  assert(clist_count(disposition->dsp_parms) == 1);
  parameter = disposition_parameter_at(disposition, 0);
  assert(parameter->pa_type == MAILMIME_DISPOSITION_PARM_FILENAME);
  assert(strcmp(parameter->pa_data.pa_filename, "real name.pdf") == 0);
  mailmime_disposition_free(disposition);
}

static void check_transfer_decoders(void)
{
  const char * base64_input = "SGVsbG8sIGJhc2U2NCE=\r\n";
  const char * qp_input = "Hello=2C=20quoted=2Dprintable=21\r\nsoft=\r\n break";
  char * decoded;
  size_t decoded_len;
  size_t indx;
  int r;

  indx = 0;
  decoded = NULL;
  decoded_len = 0;
  r = mailmime_base64_body_parse(base64_input, strlen(base64_input), &indx,
      &decoded, &decoded_len);
  assert(r == MAILIMF_NO_ERROR);
  assert(indx == strlen(base64_input));
  assert(decoded_len == strlen("Hello, base64!"));
  assert(memcmp(decoded, "Hello, base64!", decoded_len) == 0);
  mailmime_decoded_part_free(decoded);

  indx = 0;
  decoded = NULL;
  decoded_len = 0;
  r = mailmime_quoted_printable_body_parse(qp_input, strlen(qp_input), &indx,
      &decoded, &decoded_len, 0);
  assert(r == MAILIMF_NO_ERROR);
  assert(indx == strlen(qp_input));
  assert(decoded_len == strlen("Hello, quoted-printable!\r\nsoft break"));
  assert(memcmp(decoded, "Hello, quoted-printable!\r\nsoft break",
        decoded_len) == 0);
  mailmime_decoded_part_free(decoded);
}

static void check_rfc2047_encoded_words(void)
{
  struct mailmime_encoded_word * word;
  char * phrase;
  size_t indx;
  int has_fwd;
  int missing_closing_quote;
  int r;

  indx = 0;
  word = NULL;
  r = mailmime_encoded_word_parse("=?US-ASCII?Q?Keith_Moore?=",
      strlen("=?US-ASCII?Q?Keith_Moore?="), &indx, &word, &has_fwd,
      &missing_closing_quote);
  assert_parse_consumes(r, indx, "=?US-ASCII?Q?Keith_Moore?=");
  assert(strcmp(word->wd_charset, "US-ASCII") == 0);
  assert(strcmp(word->wd_text, "Keith Moore") == 0);
  assert(has_fwd == 0);
  assert(missing_closing_quote == 0);
  mailmime_encoded_word_free(word);

  indx = 0;
  word = NULL;
  r = mailmime_encoded_word_parse(" =?US-ASCII?B?SGVsbG8=?=",
      strlen(" =?US-ASCII?B?SGVsbG8=?="), &indx, &word, &has_fwd,
      &missing_closing_quote);
  assert_parse_consumes(r, indx, " =?US-ASCII?B?SGVsbG8=?=");
  assert(strcmp(word->wd_text, "Hello") == 0);
  assert(has_fwd == 1);
  mailmime_encoded_word_free(word);

  indx = 0;
  phrase = NULL;
  r = mailmime_encoded_phrase_parse("us-ascii",
      "=?ISO-8859-1?Q?Andr=E9?= Pirard",
      strlen("=?ISO-8859-1?Q?Andr=E9?= Pirard"), &indx, "utf-8", &phrase);
  assert_parse_consumes(r, indx, "=?ISO-8859-1?Q?Andr=E9?= Pirard");
  assert(strstr(phrase, "Pirard") != NULL);
  free(phrase);

  indx = 0;
  phrase = NULL;
  r = mailmime_encoded_phrase_parse("us-ascii",
      "=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=\r\n"
      " =?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=",
      strlen("=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=\r\n"
        " =?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?="),
      &indx, "utf-8", &phrase);
  assert(r == MAILIMF_NO_ERROR);
  assert(strstr(phrase, "understand the example.") != NULL);
  free(phrase);
}

static void check_message_file(const char * path, int expected_subparts)
{
  struct mailmime * mime;
  struct mailmime * message_mime;
  MMAPString * input;
  size_t indx;
  int r;

  printf("testing %s\n", path);

  input = read_fixture(path);
  indx = 0;
  mime = NULL;
  r = mailmime_parse(input->str, input->len, &indx, &mime);
  assert(r == MAILIMF_NO_ERROR);
  assert(indx == input->len);
  assert(mime != NULL);
  assert(mime->mm_type == MAILMIME_MESSAGE);
  assert(mime->mm_data.mm_message.mm_fields != NULL);

  message_mime = mime->mm_data.mm_message.mm_msg_mime;
  assert(message_mime != NULL);
  assert(message_mime->mm_type == MAILMIME_MULTIPLE);
  assert(clist_count(message_mime->mm_data.mm_multipart.mm_mp_list) ==
      expected_subparts);

  mailmime_free(mime);
  mmap_string_free(input);
}

static void check_full_rfc822_multipart(void)
{
  struct mailmime * mime;
  struct mailmime * message_mime;
  struct mailmime * part;
  MMAPString * input;
  char * decoded;
  size_t decoded_len;
  size_t indx;
  int r;

  check_message_file("data/messages/rfc822-multipart.eml", 3);
  check_message_file("data/messages/rfc822-alternative.eml", 2);

  input = read_fixture("data/messages/rfc822-multipart.eml");
  indx = 0;
  mime = NULL;
  r = mailmime_parse(input->str, input->len, &indx, &mime);
  assert(r == MAILIMF_NO_ERROR);

  message_mime = mime->mm_data.mm_message.mm_msg_mime;
  assert(message_mime->mm_data.mm_multipart.mm_preamble != NULL);

  part = clist_nth_data(message_mime->mm_data.mm_multipart.mm_mp_list, 0);
  assert(part != NULL);
  assert(part->mm_type == MAILMIME_SINGLE);
  assert(part->mm_content_type->ct_type->tp_data.tp_discrete_type->dt_type ==
      MAILMIME_DISCRETE_TYPE_TEXT);
  indx = 0;
  decoded = NULL;
  decoded_len = 0;
  r = mailmime_part_parse(part->mm_body->dt_data.dt_text.dt_data,
      part->mm_body->dt_data.dt_text.dt_length, &indx,
      part->mm_body->dt_encoding, &decoded, &decoded_len);
  assert(r == MAILIMF_NO_ERROR);
  assert(strstr(decoded, "Hello, world!") != NULL);
  assert(strstr(decoded, "soft break") != NULL);
  mailmime_decoded_part_free(decoded);

  part = clist_nth_data(message_mime->mm_data.mm_multipart.mm_mp_list, 1);
  assert(part != NULL);
  assert(part->mm_type == MAILMIME_MESSAGE);
  assert(part->mm_data.mm_message.mm_fields != NULL);
  assert(part->mm_data.mm_message.mm_msg_mime != NULL);
  assert(part->mm_data.mm_message.mm_msg_mime->mm_type == MAILMIME_SINGLE);

  part = clist_nth_data(message_mime->mm_data.mm_multipart.mm_mp_list, 2);
  assert(part != NULL);
  assert(part->mm_type == MAILMIME_SINGLE);
  assert(part->mm_body->dt_encoding == MAILMIME_MECHANISM_BASE64);
  indx = 0;
  decoded = NULL;
  decoded_len = 0;
  r = mailmime_part_parse(part->mm_body->dt_data.dt_text.dt_data,
      part->mm_body->dt_data.dt_text.dt_length, &indx,
      part->mm_body->dt_encoding, &decoded, &decoded_len);
  assert(r == MAILIMF_NO_ERROR);
  assert(decoded_len == strlen("Hello, base64!"));
  assert(memcmp(decoded, "Hello, base64!", decoded_len) == 0);
  mailmime_decoded_part_free(decoded);

  mailmime_free(mime);
  mmap_string_free(input);
}

int mime_parser_test_run(void)
{
  check_content_type_grammar();
  check_encoding_grammar();
  check_field_grammar();
  check_encoded_structured_field_recovery();
  check_rfc2231_content_type_parameters();
  check_rfc2231_content_disposition_filename();
  check_transfer_decoders();
  check_rfc2047_encoded_words();
  check_full_rfc822_multipart();

  puts("parser_test: ok");
  return 0;
}

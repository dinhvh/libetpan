#include "mime_builder_helpers.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "clist.h"
#include "mailimf_types_helper.h"
#include "mailmime_types_helper.h"
#include "test_utils.h"

static struct mailimf_fields * new_message_fields(const char * subject,
    int multiple_recipients)
{
  struct mailimf_fields * fields;
  struct mailimf_field * field;
  int r;

  fields = mailimf_fields_new_empty();
  assert(fields != NULL);

#define ADD_FIELD(name, value) \
  do { \
    field = mailimf_field_new_custom(strdup(name), strdup(value)); \
    assert(field != NULL); \
    r = mailimf_fields_add(fields, field); \
    assert(r == MAILIMF_NO_ERROR); \
  } while (0)

  ADD_FIELD("Date", "Sun, 31 Dec 2000 16:00:00 -0800");
  ADD_FIELD("From", "=?utf-8?Q?Ho=C3=A0?= <dinh.viet.hoa@gmail.com>");
  if (multiple_recipients) {
    ADD_FIELD("To", "Foo Bar <dinh.viet.hoa@gmail.com>, Other Recipient\r\n"
        " <another-foobar@to-recipient.org>");
    ADD_FIELD("Cc", "Carbon Copy <dinh.viet.hoa@gmail.com>, Other Recipient\r\n"
        " <another-foobar@to-recipient.org>");
  }
  else {
    ADD_FIELD("To", "Foo Bar <dinh.viet.hoa@gmail.com>");
  }
  ADD_FIELD("Message-ID", "<MyMessageID123@mail.gmail.com>");
  ADD_FIELD("Subject", subject);

#undef ADD_FIELD

  return fields;
}

struct mailmime * mime_builder_new_part(int type, const char * content_type,
    struct mailmime_fields * fields)
{
  struct mailmime_content * content;
  struct mailmime * mime;
  clist * list = NULL;

  content = mailmime_content_new_with_str(content_type);
  assert(content != NULL);
  if (fields == NULL) {
    fields = mailmime_fields_new_empty();
    assert(fields != NULL);
  }
  if (type == MAILMIME_MULTIPLE) {
    list = clist_new();
    assert(list != NULL);
  }

  mime = mailmime_new(type, NULL, 0, fields, content, NULL, NULL, NULL,
      list, NULL, NULL);
  assert(mime != NULL);
  return mime;
}

struct mailmime_fields * mime_builder_new_body_fields(int encoding,
    int disposition_type, const char * filename, const char * content_id)
{
  struct mailmime_mechanism * mechanism;
  struct mailmime_disposition * disposition;
  struct mailmime_fields * fields;

  mechanism = mailmime_mechanism_new(encoding, NULL);
  assert(mechanism != NULL);
  disposition = mailmime_disposition_new_with_data(disposition_type,
      filename == NULL ? NULL : strdup(filename), NULL, NULL, NULL,
      (size_t) -1);
  assert(disposition != NULL);
  fields = mailmime_fields_new_with_data(mechanism,
      content_id == NULL ? NULL : strdup(content_id), NULL, disposition, NULL);
  assert(fields != NULL);
  return fields;
}

struct mailmime * mime_builder_new_text_part(const char * content_type,
    int encoding, const char * body)
{
  struct mailmime * part;
  int r;

  part = mime_builder_new_part(MAILMIME_SINGLE, content_type,
      mime_builder_new_body_fields(encoding, MAILMIME_DISPOSITION_TYPE_INLINE,
          NULL, NULL));
  r = mailmime_set_body_text(part, (char *) body, strlen(body));
  assert(r == MAILIMF_NO_ERROR);
  return part;
}

struct mailmime * mime_builder_new_attachment_part(const char * filename,
    const char * content_id)
{
  struct mailmime * part;
  int r;

  part = mime_builder_new_part(MAILMIME_SINGLE, "image/jpeg",
      mime_builder_new_body_fields(MAILMIME_MECHANISM_BASE64,
          MAILMIME_DISPOSITION_TYPE_ATTACHMENT, test_basename(filename),
          content_id));
  r = mailmime_set_body_file(part, strdup(filename));
  assert(r == MAILIMF_NO_ERROR);
  return part;
}

void mime_builder_add_part(struct mailmime * parent, struct mailmime * child)
{
  int r;

  r = mailmime_add_part(parent, child);
  assert(r == MAILIMF_NO_ERROR);
}

struct mailmime * mime_builder_wrap_message(struct mailmime * body,
    const char * subject, int multiple_recipients)
{
  struct mailmime * message;

  message = mailmime_new_message_data(body);
  assert(message != NULL);
  mailmime_set_imf_fields(message,
      new_message_fields(subject, multiple_recipients));
  return message;
}

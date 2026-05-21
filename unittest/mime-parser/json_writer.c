#include "json_writer.h"

#include <assert.h>
#include <stdio.h>

void json_writer_append(struct json_writer * writer, const char * str)
{
  assert(mmap_string_append(writer->out, str) != NULL);
}

void json_writer_comma_if_needed(struct json_writer * writer)
{
  char last;

  if (writer->out->len == 0)
    return;
  last = writer->out->str[writer->out->len - 1];
  if (last != '{' && last != '[' && last != ',')
    json_writer_append(writer, ",");
}

void json_writer_string(struct json_writer * writer, const char * str)
{
  const unsigned char * p = (const unsigned char *) str;

  json_writer_append(writer, "\"");
  while (*p != '\0') {
    if (*p == '"' || *p == '\\') {
      assert(mmap_string_append_c(writer->out, '\\') != NULL);
      assert(mmap_string_append_c(writer->out, (char) *p) != NULL);
      p++;
    }
    else if (*p == '/') {
      json_writer_append(writer, "\\/");
      p++;
    }
    else if (*p < 0x20) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", *p);
      json_writer_append(writer, buf);
      p++;
    }
    else if (*p < 0x80) {
      assert(mmap_string_append_c(writer->out, (char) *p) != NULL);
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
        json_writer_append(writer, buf);
      }
    }
  }
  json_writer_append(writer, "\"");
}

void json_writer_time(struct json_writer * writer, time_t value)
{
  char buf[32];

  if (value < 0)
    snprintf(buf, sizeof(buf), "%u", (unsigned int) value);
  else
    snprintf(buf, sizeof(buf), "%ld", (long) value);
  json_writer_string(writer, buf);
}

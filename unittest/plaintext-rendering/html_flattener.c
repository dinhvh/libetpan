#include "html_flattener.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <libxml/HTMLparser.h>
#include <libxml/xmlerror.h>

#include "html_cleaner.h"
#include "mmapstring.h"

void plaintext_rendering_html_flattener_init(void)
{
  xmlInitParser();
}

void plaintext_rendering_html_flattener_cleanup(void)
{
  xmlCleanupParser();
}

struct link_item {
  char * href;
  size_t offset;
};

struct flatten_state {
  int level;
  int enabled;
  int disabled_level;
  MMAPString * result;
  int has_quote;
  int quote_level;
  int has_text;
  int last_char_is_whitespace;
  int show_blockquote;
  int show_link;
  int has_return_to_line;
  struct link_item links[128];
  unsigned int link_count;
  int paragraph_spacing[128];
  unsigned int paragraph_count;
};

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

static int mc_is_whitespace(unsigned int ch)
{
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\f' ||
      ch == '\r';
}

static void append_quote(struct flatten_state * state)
{
  int i;

  for (i = 0; i < state->quote_level; i++)
    append_str(state->result, "> ");
  state->last_char_is_whitespace = 1;
}

static void clean_terminal_space(MMAPString * result)
{
  if (result->len > 0 && result->str[result->len - 1] == ' ')
    mmap_string_truncate(result, result->len - 1);
}

static int is_previous_line_blank_line(MMAPString * result)
{
  if (result->len < 2)
    return 0;
  return result->str[result->len - 1] == '\n' &&
      result->str[result->len - 2] == '\n';
}

static void return_to_line(struct flatten_state * state)
{
  if (!state->has_quote) {
    append_quote(state);
    state->has_quote = 1;
  }
  clean_terminal_space(state->result);
  if (!is_previous_line_blank_line(state->result))
    append_char(state->result, '\n');
  state->has_text = 0;
  state->last_char_is_whitespace = 1;
  state->has_quote = 0;
  state->has_return_to_line = 0;
}

static void return_to_line_at_beginning_of_block(struct flatten_state * state)
{
  if (state->has_text)
    return_to_line(state);
  state->has_quote = 0;
}

static int is_block_element(const char * name)
{
  static const char * elements[] = {
    "address", "div", "p", "h1", "h2", "h3", "h4", "h5", "h6",
    "pre", "ul", "ol", "li", "dl", "dt", "dd", "form", "col",
    "colgroup", "th", "tbody", "thead", "tfoot", "table", "tr", "td",
    NULL
  };
  unsigned int i;

  for (i = 0; elements[i] != NULL; i++) {
    if (strcasecmp(name, elements[i]) == 0)
      return 1;
  }
  return 0;
}

static char * attr_value(const xmlChar ** atts, const char * name)
{
  const xmlChar ** cur;

  if (atts == NULL)
    return NULL;
  for (cur = atts; *cur != NULL && cur[1] != NULL; cur += 2) {
    if (strcasecmp((const char *) cur[0], name) == 0)
      return strdup((const char *) cur[1]);
  }
  return NULL;
}

static void characters_parsed(void * context, const xmlChar * ch, int len)
{
  struct flatten_state * state = context;
  char * text;
  char * start;
  char * end;
  int has_initial_whitespace = 0;
  int has_terminal_whitespace = 0;

  if (!state->enabled)
    return;
  text = malloc((size_t) len + 1);
  assert(text != NULL);
  memcpy(text, ch, (size_t) len);
  text[len] = '\0';
  {
    char * read = text;
    char * write = text;
    while (*read != '\0') {
      if ((unsigned char) read[0] == 0xc2 &&
          (unsigned char) read[1] == 0xa0) {
        *write++ = ' ';
        read += 2;
      }
      else {
        *write++ = *read++;
      }
    }
    *write = '\0';
    len = (int) strlen(text);
  }
  if (len > 0) {
    has_initial_whitespace = mc_is_whitespace((unsigned char) text[0]);
    has_terminal_whitespace = mc_is_whitespace((unsigned char) text[len - 1]);
  }
  start = text;
  while (*start != '\0' && mc_is_whitespace((unsigned char) *start))
    start++;
  end = start + strlen(start);
  while (end > start && mc_is_whitespace((unsigned char) end[-1]))
    end--;
  *end = '\0';
  {
    char * collapsed = collapse_spaces(start);
    strcpy(start, collapsed);
    free(collapsed);
  }
  if (has_terminal_whitespace) {
    size_t l = strlen(start);
    if (l == 0 || start[l - 1] != ' ') {
      start[l] = ' ';
      start[l + 1] = '\0';
    }
  }
  if (*start != '\0') {
    int last_is_whitespace = start[strlen(start) - 1] == ' ';
    int only_whitespace = last_is_whitespace && strlen(start) == 1;
    if (only_whitespace) {
      if (!state->last_char_is_whitespace && state->has_text) {
        append_str(state->result, " ");
        state->last_char_is_whitespace = 1;
        state->has_text = 1;
      }
    }
    else {
      if (!state->has_quote) {
        append_quote(state);
        state->has_quote = 1;
      }
      if (has_initial_whitespace && !state->last_char_is_whitespace)
        append_str(state->result, " ");
      append_str(state->result, start);
      state->last_char_is_whitespace = last_is_whitespace;
      state->has_text = 1;
    }
  }
  free(text);
}

static void element_started(void * ctx, const xmlChar * name,
    const xmlChar ** atts)
{
  struct flatten_state * state = ctx;
  const char * tag = (const char *) name;

  if (strcasecmp(tag, "blockquote") == 0) {
    state->quote_level++;
  }
  else if (strcasecmp(tag, "a") == 0) {
    char * href = attr_value(atts, "href");
    if (state->link_count < 128) {
      state->links[state->link_count].href = href != NULL ? href : strdup("");
      state->links[state->link_count].offset = state->result->len;
      state->link_count++;
    }
    else {
      free(href);
    }
  }
  else if (strcasecmp(tag, "p") == 0) {
    int has_spacing = 1;
    char * style = attr_value(atts, "style");
    if (style != NULL) {
      if (strstr(style, "margin: 0.0px 0.0px 0.0px 0.0px;") != NULL)
        has_spacing = 0;
      else if (strstr(style, "margin: 0px 0px 0px 0px;") != NULL)
        has_spacing = 0;
      else if (strstr(style, "margin: 0.0px;") != NULL)
        has_spacing = 0;
      else if (strstr(style, "margin: 0px;") != NULL)
        has_spacing = 0;
    }
    free(style);
    if (state->paragraph_count < 128)
      state->paragraph_spacing[state->paragraph_count++] = has_spacing;
    if (state->enabled)
      return_to_line_at_beginning_of_block(state);
    if (state->enabled && has_spacing)
      return_to_line(state);
  }
  else if (strcasecmp(tag, "br") == 0) {
    if (state->enabled) {
      return_to_line(state);
      state->has_return_to_line = 1;
    }
  }
  else if (is_block_element(tag)) {
    if (state->enabled)
      return_to_line_at_beginning_of_block(state);
  }
  if (strcasecmp(tag, "style") == 0 || strcasecmp(tag, "script") == 0) {
    state->enabled = 0;
    state->disabled_level = state->level;
  }
  else if (strcasecmp(tag, "title") == 0) {
    state->enabled = 0;
    state->disabled_level = state->level;
  }
  state->level++;
}

static void element_ended(void * ctx, const xmlChar * name)
{
  struct flatten_state * state = ctx;
  const char * tag = (const char *) name;
  int has_return_to_line = 0;

  state->level--;
  if (!state->enabled && state->level == state->disabled_level)
    state->enabled = 1;
  if (strcasecmp(tag, "blockquote") == 0) {
    if (state->quote_level > 0)
      state->quote_level--;
    has_return_to_line = 1;
  }
  else if (strcasecmp(tag, "a") == 0) {
    if (state->link_count > 0) {
      struct link_item * item = &state->links[state->link_count - 1];
      if (state->show_link && item->offset != state->result->len &&
          item->href[0] != '\0') {
        size_t href_len = strlen(item->href);
        if (!(state->result->len >= href_len &&
            strcmp(state->result->str + state->result->len - href_len,
            item->href) == 0)) {
          if (!state->last_char_is_whitespace)
            append_str(state->result, " ");
          append_str(state->result, "(");
          append_str(state->result, item->href);
          append_str(state->result, ")");
          state->has_text = 1;
          state->last_char_is_whitespace = 0;
        }
      }
      free(item->href);
      state->link_count--;
    }
  }
  else if (strcasecmp(tag, "p") == 0) {
    if (state->enabled && state->paragraph_count > 0 &&
        state->paragraph_spacing[state->paragraph_count - 1])
      return_to_line(state);
    if (state->paragraph_count > 0)
      state->paragraph_count--;
    has_return_to_line = 1;
  }
  else if (is_block_element(tag)) {
    has_return_to_line = 1;
  }
  if (has_return_to_line && state->enabled && !state->has_return_to_line)
    return_to_line(state);
}

static void comment_parsed(void * ctx, const xmlChar * value)
{
  (void) ctx;
  (void) value;
}

static void structured_error(void * user_data, xmlErrorPtr error)
{
  (void) user_data;
  (void) error;
}

char * plaintext_rendering_flatten_html(const char * html)
{
  xmlSAXHandler handler;
  struct flatten_state state;
  char * cleaned_html;
  unsigned int i;

  memset(&handler, 0, sizeof(handler));
  memset(&state, 0, sizeof(state));
  handler.characters = characters_parsed;
  handler.startElement = element_started;
  handler.endElement = element_ended;
  handler.comment = comment_parsed;
  state.result = mmap_string_new("");
  assert(state.result != NULL);
  state.enabled = 1;
  state.show_blockquote = 1;
  state.show_link = 1;
  state.last_char_is_whitespace = 1;
  xmlSetStructuredErrorFunc(xmlGenericErrorContext, structured_error);
  cleaned_html = plaintext_rendering_cleaned_html(html);
  htmlSAXParseDoc((xmlChar *) cleaned_html, "utf-8", &handler, &state);
  free(cleaned_html);
  clean_terminal_space(state.result);
  for (i = 0; i < state.link_count; i++)
    free(state.links[i].href);
  return detach_mmap_string(state.result);
}

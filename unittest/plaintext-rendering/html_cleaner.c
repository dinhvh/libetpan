#include "html_cleaner.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <tidy.h>
#include <tidybuffio.h>

char * plaintext_rendering_cleaned_html(const char * html)
{
  TidyDoc doc;
  TidyBuffer input;
  TidyBuffer output;
  TidyBuffer errors;
  char * result;

  doc = tidyCreate();
  tidyBufInit(&input);
  tidyBufInit(&output);
  tidyBufInit(&errors);
  tidyBufAppend(&input, (void *) html, (unsigned int) strlen(html));
  tidyOptSetBool(doc, TidyXhtmlOut, yes);
  tidyOptSetInt(doc, TidyDoctypeMode, TidyDoctypeUser);
  tidyOptSetBool(doc, TidyMark, no);
  tidySetCharEncoding(doc, "utf8");
  tidyOptSetBool(doc, TidyForceOutput, yes);
  tidyOptSetBool(doc, TidyShowWarnings, no);
  tidyOptSetInt(doc, TidyShowErrors, 0);
  tidySetErrorBuffer(doc, &errors);
  tidyParseBuffer(doc, &input);
  tidyCleanAndRepair(doc);
  tidySaveBuffer(doc, &output);
  result = strdup((const char *) output.bp);
  assert(result != NULL);
  tidyBufFree(&input);
  tidyBufFree(&output);
  tidyBufFree(&errors);
  tidyRelease(doc);
  return result;
}

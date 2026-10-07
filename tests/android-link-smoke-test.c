/*
 * Android static-link smoke coverage for dependency-backed low-level features.
 *
 * This file is cross-linked by tests/android-link-smoke-test.sh. It is not
 * executed; unresolved symbols are the signal.
 */

#include <stddef.h>
#include <stdlib.h>

#include <curl/curl.h>
#include <unicode/ucnv.h>
#include <libxml/parser.h>
#include <sasl/sasl.h>

#include <libetpan/mailjson.h>
#include <libetpan/charconv.h>
#include <libetpan/mailpgp.h>
#include <libetpan/mailstream_ssl.h>

#include "../src/low-level/feed/newsfeed.h"

static void * volatile link_smoke_sink;

int android_link_smoke_touch(void)
{
  struct newsfeed * feed;
  struct mailpgp * pgp;
  mailjson_value * json;
  UConverter * converter;
  UErrorCode conversion_error = U_ZERO_ERROR;
  char * converted = NULL;
  int result;

  result = 0;

  result += mailstream_ssl_backend_is_available(MAILSTREAM_SSL_BACKEND_OPENSSL);
  result += mailstream_ssl_set_backend(MAILSTREAM_SSL_BACKEND_OPENSSL);

  result += sasl_client_init(NULL);
  sasl_done();

  converter = ucnv_open("UTF-8", &conversion_error);
  if (converter != NULL) {
    ucnv_close(converter);
  }
  result += charconv("UTF-8", "ISO-8859-16", "\252", 1, &converted);
  free(converted);

  if (mailjson_new_object(&json) == MAILJSON_NO_ERROR) {
    mailjson_free(json);
  }

  result += curl_global_init(CURL_GLOBAL_DEFAULT);
  curl_global_cleanup();

  feed = newsfeed_new();
  if (feed != NULL) {
    result += newsfeed_set_url(feed, "https://example.test/feed.xml");
    result += newsfeed_update(feed, (time_t) -1);
    newsfeed_free(feed);
  }

  link_smoke_sink = xmlReadMemory("<rss/>", 6, "smoke.xml", NULL, 0);
  if (link_smoke_sink != NULL) {
    xmlFreeDoc((xmlDocPtr) link_smoke_sink);
    link_smoke_sink = NULL;
  }

  pgp = mailpgp_new();
  if (pgp != NULL) {
    mailpgp_free(pgp);
  }

  return result;
}

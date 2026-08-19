#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libetpan/mailhttp.h>
#include "../../src/low-level/feed/newsfeed.h"

#include <stdio.h>
#include <string.h>

static int fake_perform(struct mailhttp_transport * transport,
    struct mailhttp_request * request,
    struct mailhttp_response ** result)
{
  static const char xml[] =
      "<?xml version=\"1.0\"?><rss version=\"2.0\"><channel>"
      "<title>Example</title><description>Feed</description>"
      "<link>https://example.test/</link><item><title>Item</title>"
      "<link>https://example.test/item</link></item></channel></rss>";
  struct mailhttp_response * response;
  (void) transport;

  if ((strcmp(request->method, "GET") != 0) ||
      (request->body_sink == NULL))
    return MAILHTTP_ERROR_PROTOCOL;
  if (request->body_sink(xml, sizeof(xml) - 1,
      request->body_sink_context) != 0)
    return MAILHTTP_ERROR_BODY_SINK;
  response = mailhttp_response_new(200);
  if (response == NULL)
    return MAILHTTP_ERROR_MEMORY;
  * result = response;
  return MAILHTTP_NO_ERROR;
}

static const struct mailhttp_transport_driver fake_driver = {
  fake_perform,
  NULL
};

int main(void)
{
  struct mailhttp_transport * transport;
  struct newsfeed * feed;
  int r;

  transport = mailhttp_transport_new(NULL, &fake_driver,
      MAILHTTP_BACKEND_CURL);
  feed = newsfeed_new();
  if ((transport == NULL) || (feed == NULL))
    return 1;
  if (newsfeed_set_http_transport(feed, transport) != NEWSFEED_NO_ERROR)
    return 1;
  if (newsfeed_set_url(feed, "https://example.test/feed") !=
      NEWSFEED_NO_ERROR)
    return 1;
  r = newsfeed_update(feed, -1);
  if ((r != NEWSFEED_NO_ERROR) ||
      (newsfeed_get_response_code(feed) != 200) ||
      (newsfeed_item_list_get_count(feed) != 1) ||
      (strcmp(newsfeed_get_title(feed), "Example") != 0)) {
    fprintf(stderr, "feed HTTP transport test failed: %d\n", r);
    newsfeed_free(feed);
    return 1;
  }
  newsfeed_free(feed);
  printf("feed HTTP transport test passed\n");
  return 0;
}

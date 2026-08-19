/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * Copyright (C) 2006 Andrej Kacian <andrej@kacian.sk>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "newsfeed.h"

#include <string.h>
#include <stdlib.h>

#include "newsfeed_private.h"
#include "mailstream.h"
#include "newsfeed_item.h"
#include "parser.h"
#include "mailhttp.h"

#ifdef LIBETPAN_REENTRANT
#	ifndef WIN32
#		include <pthread.h>
#	endif
#endif

static int mailhttp_error_convert(int error);

static int newsfeed_body_sink(const void * data, size_t length, void * context)
{
  return newsfeed_writefunc((void *) data, 1, length, context) == length ?
      0 : -1;
}

/* feed_new()
 * Initializes new Feed struct, setting its url and a default timeout. */
struct newsfeed * newsfeed_new(void)
{
  struct newsfeed * feed;
  
  feed = malloc(sizeof(* feed));
  if (feed == NULL)
    goto err;
  
  feed->feed_url = NULL;
  feed->feed_title = NULL;
  feed->feed_description = NULL;
  feed->feed_language = NULL;
  feed->feed_author = NULL;
  feed->feed_generator = NULL;
  feed->feed_item_list = carray_new(16);
  if (feed->feed_item_list == NULL)
    goto free;
  feed->feed_response_code = 0;
  feed->feed_timeout = 0;
  feed->feed_http_transport = NULL;
  (void) mailhttp_transport_new_default(&feed->feed_http_transport);
  
  return feed;
  
 free:
  free(feed);
 err:
  return NULL;
}

void newsfeed_free(struct newsfeed * feed)
{
  unsigned int i;
  
  free(feed->feed_url);
  free(feed->feed_title);
  free(feed->feed_description);
  free(feed->feed_language);
  free(feed->feed_author);
  free(feed->feed_generator);
  mailhttp_transport_free(feed->feed_http_transport);
  
  for(i = 0 ; i < carray_count(feed->feed_item_list) ; i ++) {
    struct newsfeed_item * item;
    
    item = carray_get(feed->feed_item_list, i);
    newsfeed_item_free(item);
  }
  
  free(feed);
}

int newsfeed_get_response_code(struct newsfeed * feed)
{
  return feed->feed_response_code;
}

/* URL */
int newsfeed_set_url(struct newsfeed * feed, const char * url)
{
  if (url != feed->feed_url) {
    char * dup_url;
    
    if (url == NULL) {
      dup_url = NULL;
    }
    else {
      dup_url = strdup(url);
      if (dup_url == NULL)
        return -1;
    }
    
    free(feed->feed_url);
    feed->feed_url = dup_url;
  }
  
  return 0;
}

const char * newsfeed_get_url(struct newsfeed * feed)
{
  return feed->feed_url;
}

/* Title */
int newsfeed_set_title(struct newsfeed * feed, const char * title)
{
  if (title != feed->feed_title) {
    char * dup_title;
    
    if (title == NULL) {
      dup_title = NULL;
    }
    else {
      dup_title = strdup(title);
      if (dup_title == NULL)
        return -1;
    }
    
    free(feed->feed_title);
    feed->feed_title = dup_title;
  }
  
  return 0;
}

const char * newsfeed_get_title(struct newsfeed *feed)
{
  return feed->feed_title;
}

/* Description */
int newsfeed_set_description(struct newsfeed * feed, const char * description)
{
  if (description != feed->feed_description) {
    char * dup_description;
    
    if (description == NULL) {
      dup_description = NULL;
    }
    else {
      dup_description = strdup(description);
      if (dup_description == NULL)
        return -1;
    }
    
    free(feed->feed_description);
    feed->feed_description = dup_description;
  }
  
  return 0;
}

const char * newsfeed_get_description(struct newsfeed * feed)
{
  return feed->feed_description;
}

/* Language */
int newsfeed_set_language(struct newsfeed * feed, const char * language)
{
  if (language != feed->feed_language) {
    char * dup_language;
    
    if (language == NULL) {
      dup_language = NULL;
    }
    else {
      dup_language = strdup(language);
      if (dup_language == NULL)
        return -1;
    }
    
    free(feed->feed_language);
    feed->feed_language = dup_language;
  }
  
  return 0;
}

const char * newsfeed_get_language(struct newsfeed * feed)
{
  return feed->feed_language;
}

/* Author */
int newsfeed_set_author(struct newsfeed * feed, const char * author)
{
  if (author != feed->feed_author) {
    char * dup_author;
    
    if (author == NULL) {
      dup_author = NULL;
    }
    else {
      dup_author = strdup(author);
      if (dup_author == NULL)
        return -1;
    }
    
    free(feed->feed_author);
    feed->feed_author = dup_author;
  }
  
  return 0;
}

const char * newsfeed_get_author(struct newsfeed * feed)
{
  return feed->feed_author;
}

/* Generator */
int newsfeed_set_generator(struct newsfeed * feed, const char * generator)
{
  if (generator != feed->feed_generator) {
    char * dup_generator;
    
    if (generator == NULL) {
      dup_generator = NULL;
    }
    else {
      dup_generator = strdup(generator);
      if (dup_generator == NULL)
        return -1;
    }
    
    free(feed->feed_generator);
    feed->feed_generator = dup_generator;
  }
  
  return 0;
}

const char * newsfeed_get_generator(struct newsfeed * feed)
{
  return feed->feed_generator;
}

void newsfeed_set_date(struct newsfeed * feed, time_t date)
{
  feed->feed_date = date;
}

time_t newsfeed_get_date(struct newsfeed * feed)
{
  return feed->feed_date;
}

/* Returns nth item from feed. */
unsigned int newsfeed_item_list_get_count(struct newsfeed * feed)
{
  return carray_count(feed->feed_item_list);
}

struct newsfeed_item * newsfeed_get_item(struct newsfeed * feed, unsigned int n)
{
  return carray_get(feed->feed_item_list, n);
}

/* feed_update()
 * Takes initialized feed with url set, fetches the feed from this url,
 * updates rest of Feed struct members and returns HTTP response code
 * we got from url's server. */
int newsfeed_update(struct newsfeed * feed, time_t last_update)
{
  struct mailhttp_transport * transport = NULL;
  struct mailhttp_request * request = NULL;
  struct mailhttp_response * response = NULL;
  struct newsfeed_parser_context * feed_ctx;
  unsigned int res;
  time_t timeout_value;
  int r;
  
  if (feed->feed_url == NULL) {
    res = NEWSFEED_ERROR_BADURL;
    goto err;
  }
  
  transport = feed->feed_http_transport;
  if (transport == NULL) {
    res = NEWSFEED_ERROR_INTERNAL;
    goto err;
  }

  feed_ctx = malloc(sizeof(* feed_ctx));
  if (feed_ctx == NULL) {
    res = NEWSFEED_ERROR_MEMORY;
    goto err;
  }
  
  res = newsfeed_parser_context_init(feed_ctx, feed);
  if (res != NEWSFEED_NO_ERROR)
    goto free_ctx;
  
  if (feed->feed_timeout != 0)
    timeout_value = feed->feed_timeout;
  else
    timeout_value = mailstream_network_delay.tv_sec;
  
  request = mailhttp_request_new("GET", feed->feed_url);
  if (request == NULL) {
    res = NEWSFEED_ERROR_MEMORY;
    goto free_parser;
  }
  request->timeout = timeout_value;
  request->max_redirects = 3;
  mailhttp_request_set_body_sink(request, newsfeed_body_sink, feed_ctx);
  r = mailhttp_request_add_header(request, "User-Agent", "libEtPan!");
  if (r != MAILHTTP_NO_ERROR) {
    res = mailhttp_error_convert(r);
    goto free_request;
  }

  if (last_update != -1) {
    char date[64];
    struct tm * tm_value = gmtime(&last_update);
    if ((tm_value != NULL) &&
        (strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT",
            tm_value) != 0)) {
      r = mailhttp_request_add_header(request, "If-Modified-Since", date);
      if (r != MAILHTTP_NO_ERROR) {
        res = mailhttp_error_convert(r);
        goto free_request;
      }
    }
  }

  r = mailhttp_perform(transport, request, &response);
  if (r != MAILHTTP_NO_ERROR) {
    res = feed_ctx->error != NEWSFEED_NO_ERROR ? feed_ctx->error :
        mailhttp_error_convert(r);
    goto free_request;
  }

  res = newsfeed_parser_end(feed_ctx);
  if (res != NEWSFEED_NO_ERROR)
    goto free_response;
  
  if (feed_ctx->error != NEWSFEED_NO_ERROR) {
    res = feed_ctx->error;
    goto free_response;
  }

  feed->feed_response_code = response->status_code;
  mailhttp_response_free(response);
  mailhttp_request_free(request);
  newsfeed_parser_context_cleanup(feed_ctx);
  free(feed_ctx);
  return NEWSFEED_NO_ERROR;

 free_response:
  mailhttp_response_free(response);
 free_request:
  mailhttp_request_free(request);
 free_parser:
  newsfeed_parser_context_cleanup(feed_ctx);
 free_ctx:
  free(feed_ctx);
 err:
  return res;
}

int newsfeed_add_item(struct newsfeed * feed, struct newsfeed_item * item)
{
  return carray_add(feed->feed_item_list, item, NULL);
}

static int mailhttp_error_convert(int error)
{
  switch (error) {
  case MAILHTTP_NO_ERROR:
    return NEWSFEED_NO_ERROR;
  case MAILHTTP_ERROR_PROTOCOL:
    return NEWSFEED_ERROR_UNSUPPORTED_PROTOCOL;
  case MAILHTTP_ERROR_BAD_URL:
  case MAILHTTP_ERROR_BAD_STATE:
    return NEWSFEED_ERROR_BADURL;
  case MAILHTTP_ERROR_RESOLVE:
    return NEWSFEED_ERROR_RESOLVE_HOST;
  case MAILHTTP_ERROR_CONNECT:
    return NEWSFEED_ERROR_CONNECT;
  case MAILHTTP_ERROR_MEMORY:
    return NEWSFEED_ERROR_MEMORY;
  case MAILHTTP_ERROR_CANCELLED:
    return NEWSFEED_ERROR_CANCELLED;
  case MAILHTTP_ERROR_TIMEOUT:
  case MAILHTTP_ERROR_BODY_SINK:
  case MAILHTTP_ERROR_IO:
    return NEWSFEED_ERROR_STREAM;
  case MAILHTTP_ERROR_TLS:
    return NEWSFEED_ERROR_SSL;
  case MAILHTTP_ERROR_UNAVAILABLE:
  default:
    return NEWSFEED_ERROR_INTERNAL;
  }
}

void newsfeed_set_timeout(struct newsfeed * feed, unsigned int timeout)
{
  feed->feed_timeout = timeout;
}

unsigned int newsfeed_get_timeout(struct newsfeed * feed)
{
  return feed->feed_timeout;
}

int newsfeed_set_http_transport(struct newsfeed * feed,
    struct mailhttp_transport * transport)
{
  if ((feed == NULL) || (transport == NULL))
    return NEWSFEED_ERROR_INTERNAL;
  mailhttp_transport_free(feed->feed_http_transport);
  feed->feed_http_transport = transport;
  return NEWSFEED_NO_ERROR;
}

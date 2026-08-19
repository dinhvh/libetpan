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

#include "parser.h"

#ifdef HAVE_CURL
#include <limits.h>
#endif
#include <string.h>
#include <stdio.h>
#ifdef HAVE_CURL
#include <libxml/parser.h>
#endif

#include "newsfeed.h"

#include "newsfeed_private.h"
#include "parser_rss20.h"
#include "parser_rdf.h"
#include "parser_atom10.h"
#include "parser_atom03.h"

#ifdef HAVE_CURL
enum {
  FEED_TYPE_NONE,
  FEED_TYPE_RDF,
  FEED_TYPE_RSS_20,
  FEED_TYPE_ATOM_03,
  FEED_TYPE_ATOM_10
};

static void dispatch_start(struct newsfeed_parser_context * ctx,
    const char * el, const char ** attr)
{
  switch(ctx->feed_type) {
  case FEED_TYPE_RSS_20:
    newsfeed_parser_rss20_start(ctx, el, attr);
    break;
    
  case FEED_TYPE_RDF:
    newsfeed_parser_rdf_start(ctx, el, attr);
    break;
    
  case FEED_TYPE_ATOM_10:
    newsfeed_parser_atom10_start(ctx, el, attr);
    break;

  case FEED_TYPE_ATOM_03:
    newsfeed_parser_atom03_start(ctx, el, attr);
    break;
  }
}

static void dispatch_end(struct newsfeed_parser_context * ctx, const char * el)
{
  switch(ctx->feed_type) {
  case FEED_TYPE_RSS_20:
    newsfeed_parser_rss20_end(ctx, el);
    break;
    
  case FEED_TYPE_RDF:
    newsfeed_parser_rdf_end(ctx, el);
    break;
    
  case FEED_TYPE_ATOM_10:
    newsfeed_parser_atom10_end(ctx, el);
    break;

  case FEED_TYPE_ATOM_03:
    newsfeed_parser_atom03_end(ctx, el);
    break;
  }
}

static void elparse_start(void * data,
    const xmlChar * el, const xmlChar ** attr)
{
  struct newsfeed_parser_context * ctx;
  unsigned int feedtype;
  
  ctx = (struct newsfeed_parser_context *) data;
  
  if ((ctx->feed_type == FEED_TYPE_NONE) && (ctx->depth == 0)) {
    feedtype = FEED_TYPE_NONE;

    /* RSS 2.0 detected */
    if (strcasecmp((const char *) el, "rss") == 0) {
      feedtype = FEED_TYPE_RSS_20;
    }
    else if (strcasecmp((const char *) el, "rdf:RDF") == 0) {
      feedtype = FEED_TYPE_RDF;
    }
    else if (strcasecmp((const char *) el, "feed") == 0) {
      const char * version;
      
      /* ATOM feed detected, let's check version */
      version = newsfeed_parser_get_attribute_value((const char **) attr,
          "xmlns");
      if (version != NULL) {
        if (strcmp(version, "http://www.w3.org/2005/Atom") == 0)
          feedtype = FEED_TYPE_ATOM_10;
        else
          feedtype = FEED_TYPE_ATOM_03;
      }
    }

    ctx->feed_type = feedtype;
    ctx->depth ++;
    return;
  }

  dispatch_start(ctx, (const char *) el, (const char **) attr);
}

static void elparse_end(void * data, const xmlChar * el)
{
  struct newsfeed_parser_context * ctx;
  
  ctx = (struct newsfeed_parser_context *) data;

  if ((ctx->feed_type == FEED_TYPE_NONE) || (ctx->depth <= 1)) {
    mmap_string_truncate(ctx->str, 0);
    if (ctx->depth > 0)
      ctx->depth --;
    return;
  }

  dispatch_end(ctx, (const char *) el);
}

static void chparse(void * data, const xmlChar * s, int len)
{
  struct newsfeed_parser_context * ctx;
  const char * pt;
  int i;
  int blank;
  
  blank = 1;
  ctx = (struct newsfeed_parser_context *) data;
  
  /* check if the string is blank, ... */
  for(i = 0, pt = (const char *) s ; i < len ; i ++) {
    if ((* pt != ' ') && (* pt != '\t'))
      blank = 0;
    pt ++;
  }
  
  /* ... because we do not want to deal with blank strings */
  if (blank)
    return;
  
  for(i = 0, pt = (const char *) s ; i < len ; i ++) {
    /* do not append newline as first char of our string */
    if ((* pt != '\n') || (ctx->str->len != 0)) {
      if (mmap_string_append_c(ctx->str, * pt) == NULL) {
        ctx->error = NEWSFEED_ERROR_MEMORY;
        return;
      }
      pt ++;
    }
  }
}

int newsfeed_parser_context_init(struct newsfeed_parser_context * ctx,
    struct newsfeed * feed)
{
  memset(ctx, 0, sizeof(* ctx));

  ctx->str = mmap_string_sized_new(256);
  if (ctx->str == NULL)
    return NEWSFEED_ERROR_MEMORY;

  ctx->feed = feed;
  ctx->error = NEWSFEED_NO_ERROR;
  ctx->feed_type = FEED_TYPE_NONE;
  ctx->sax_handler.startElement = elparse_start;
  ctx->sax_handler.endElement = elparse_end;
  ctx->sax_handler.characters = chparse;
  ctx->parser = xmlCreatePushParserCtxt(&ctx->sax_handler, ctx,
      NULL, 0, NULL);
  if (ctx->parser == NULL) {
    mmap_string_free(ctx->str);
    ctx->str = NULL;
    return NEWSFEED_ERROR_MEMORY;
  }

  xmlCtxtUseOptions(ctx->parser, XML_PARSE_NONET);

  return NEWSFEED_NO_ERROR;
}

void newsfeed_parser_context_cleanup(struct newsfeed_parser_context * ctx)
{
  if (ctx->parser != NULL)
    xmlFreeParserCtxt(ctx->parser);
  if (ctx->str != NULL)
    mmap_string_free(ctx->str);
}

int newsfeed_parser_end(struct newsfeed_parser_context * ctx)
{
  int r;

  if (ctx->error != NEWSFEED_NO_ERROR)
    return ctx->error;

  r = xmlParseChunk(ctx->parser, NULL, 0, 1);
  if (r != 0)
    return NEWSFEED_ERROR_PARSE;

  return ctx->error;
}

size_t newsfeed_writefunc(void * ptr, size_t size, size_t nmemb, void * data)
{
  size_t len;
  size_t remaining;
  const char * chunk;
  struct newsfeed_parser_context * ctx;
  int r;
  
  ctx = data;
  len = size * nmemb;
  
  if (ctx->error != NEWSFEED_NO_ERROR) {
    return 0;
  }

  remaining = len;
  chunk = ptr;
  while (remaining > 0) {
    int chunk_len;

    if (remaining > INT_MAX)
      chunk_len = INT_MAX;
    else
      chunk_len = (int) remaining;

    r = xmlParseChunk(ctx->parser, chunk, chunk_len, 0);
    if (r != 0) {
      ctx->error = NEWSFEED_ERROR_PARSE;
      return 0;
    }

    remaining -= chunk_len;
    chunk += chunk_len;
  }
  
  if (ctx->error != NEWSFEED_NO_ERROR) {
    return 0;
  }
  
  return len;
}
#else
int newsfeed_parser_context_init(struct newsfeed_parser_context * ctx,
    struct newsfeed * feed)
{
  (void) ctx;
  (void) feed;
  return NEWSFEED_ERROR_INTERNAL;
}

void newsfeed_parser_context_cleanup(struct newsfeed_parser_context * ctx)
{
  (void) ctx;
}

int newsfeed_parser_end(struct newsfeed_parser_context * ctx)
{
  (void) ctx;
  return NEWSFEED_ERROR_INTERNAL;
}

size_t newsfeed_writefunc(void * ptr, size_t size, size_t nmemb, void * data)
{
  (void) ptr;
  (void) size;
  (void) nmemb;
  (void) data;
  return 0;
}
#endif

const char * newsfeed_parser_get_attribute_value(const char ** attr,
    const char * name)
{
  unsigned int i;
  
  if ((attr == NULL) || (name == NULL))
    return NULL;
  
  for(i = 0 ; attr[i] != NULL && attr[i + 1] != NULL ; i += 2 ) {
    if (strcmp(attr[i], name) == 0)
      return attr[i + 1];
  }
  
  /* We haven't found anything. */
  return NULL;
}

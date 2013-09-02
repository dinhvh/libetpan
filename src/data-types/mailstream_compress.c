/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2013 - DINH Viet Hoa
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

/*  Created by Ian Ragsdale on 3/8/13. */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailstream_compress.h"

#include <stddef.h>

#if !HAVE_ZLIB

mailstream_low * mailstream_low_compress_open(mailstream_low * ms)
{
  return NULL;
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <assert.h>

#include "mailstream_low.h"
#include "mailstream_low.h"
#include "mailstream_cancel.h"

#define CHUNK_SIZE 1024

static ssize_t mailstream_low_compress_read(mailstream_low * s, void * buf, size_t count);
static ssize_t mailstream_low_compress_write(mailstream_low * s, const void * buf, size_t count);
static int mailstream_low_compress_close(mailstream_low * s);
static int mailstream_low_compress_get_fd(mailstream_low * s);
static struct mailstream_cancel * mailstream_low_compress_get_cancel(mailstream_low * s);
static void mailstream_low_compress_free(mailstream_low * s);
static void mailstream_low_compress_cancel(mailstream_low * s);
static carray * mailstream_low_compress_get_certificate_chain(mailstream_low * s);

typedef struct mailstream_compress_data {
  mailstream_low * ms;
  z_stream *compress_stream;
  z_stream *decompress_stream;
  unsigned char input_buf[CHUNK_SIZE];
  unsigned char output_buf[CHUNK_SIZE];
} compress_data;

static mailstream_low_driver local_mailstream_compress_driver = {
  /* mailstream_read */ mailstream_low_compress_read,
  /* mailstream_write */ mailstream_low_compress_write,
  /* mailstream_close */ mailstream_low_compress_close,
  /* mailstream_get_fd */ mailstream_low_compress_get_fd,
  /* mailstream_free */ mailstream_low_compress_free,
  /* mailstream_cancel */ mailstream_low_compress_cancel,
  /* mailstream_get_cancel */ mailstream_low_compress_get_cancel,
  /* mailstream_get_certificate_chain */ mailstream_low_compress_get_certificate_chain,
};

mailstream_low_driver * mailstream_compress_driver = &local_mailstream_compress_driver;

mailstream_low * mailstream_low_compress_open(mailstream_low * ms)
{
  mailstream_low * s;
    
  /* stores the original mailstream */
  struct mailstream_compress_data * compress_data = malloc(sizeof(* compress_data));
  if (compress_data == NULL)
    goto err;

  /* allocate deflate state */
  compress_data->compress_stream = malloc(sizeof(z_stream));
  compress_data->compress_stream->zalloc = Z_NULL;
  compress_data->compress_stream->zfree = Z_NULL;
  compress_data->compress_stream->opaque = Z_NULL;
  /* these specific settings are very important - don't change without looking at the COMPRESS RFC */
  int ret = deflateInit2(compress_data->compress_stream, Z_BEST_SPEED, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
  if (ret != Z_OK) {
    goto free_compress_data;
  }
  compress_data->compress_stream->avail_in = 0;
  compress_data->compress_stream->avail_out = 0;

  /* allocate inflate state */
  compress_data->decompress_stream = malloc(sizeof(z_stream));
  compress_data->decompress_stream->zalloc = Z_NULL;
  compress_data->decompress_stream->zfree = Z_NULL;
  compress_data->decompress_stream->opaque = Z_NULL;
  /* these specific settings are very important - don't change without looking at the COMPRESS RFC */
  ret = inflateInit2(compress_data->decompress_stream, -15);
  if (ret != Z_OK) {
    goto free_compress_data;
  }
  compress_data->decompress_stream->avail_in = 0;
  compress_data->decompress_stream->avail_out = 0;

  compress_data->ms = ms;

  s = mailstream_low_new(compress_data, mailstream_compress_driver);
  if (s == NULL)
    goto free_compress_data;
    
  return s;
    
  free_compress_data:
  if (compress_data->compress_stream) {
    deflateEnd(compress_data->compress_stream);
    free(compress_data->compress_stream);
  }
  if (compress_data->decompress_stream) {
    inflateEnd(compress_data->decompress_stream);
    free(compress_data->decompress_stream);
  }
  free(compress_data);
  err:
  return NULL;
}

static ssize_t mailstream_low_compress_read(mailstream_low * s, void * buf, size_t count)
{
  compress_data * data = s->data;
  data->ms->timeout = s->timeout;
  z_stream * strm = data->decompress_stream;
    
  int zr;

  do {
    /* if there is no compressed data, read more */
    if (strm->avail_in == 0) {
      int read = data->ms->driver->mailstream_read(data->ms, data->input_buf, CHUNK_SIZE);
      if (read <= 0) {
        return read;
      }
      strm->avail_in = read;
      strm->next_in = data->input_buf;
    }

    /* set the output buffer */
    strm->next_out = buf;
    strm->avail_out = count;

    /* uncompress any waiting data */
    zr = inflate(strm, Z_NO_FLUSH);
  }
  /*
  it's possible that there was data in the stream, but not enough that zlib could figure
  out what to do with it. in this case, read some more and try again.
  */
  while (zr == Z_OK && strm->avail_in == 0 && strm->avail_out == count);

  /* if we got an error, return -1 to close the connection */
  if (zr < 0) {
    return -1;
  }

  /* let the client know how much data was read */
  return count - strm->avail_out;
}

static ssize_t mailstream_low_compress_write(mailstream_low * s, const void * buf, size_t count) {
    
  int zr;
  //int wr;
  compress_data * data = s->data;
  data->ms->timeout = s->timeout;
  z_stream * strm = data->compress_stream;

  strm->next_in = (Bytef *)buf;
  /* we won't try to compress more than CHUNK_SIZE at a time so we always have enough buffer space */
  int compress_len = MIN(count, CHUNK_SIZE);
  strm->avail_in = compress_len;
  strm->avail_out = CHUNK_SIZE;
  strm->next_out = data->output_buf;

  zr = deflate(strm, Z_PARTIAL_FLUSH);
  assert(zr == Z_OK);
  if (zr < 0) {
    printf("Error deflating: %d %s", zr, strm->msg);
    return -1;
  }
  
  unsigned char * p = data->output_buf;
  size_t remaining = CHUNK_SIZE - strm->avail_out;
  while (remaining > 0) {
    ssize_t wr = data->ms->driver->mailstream_write(data->ms, p, remaining);
    if (wr < 0) {
      return -1;
    }
    
    p += wr;
    remaining -= wr;
  }
  
  /* let the caller know how much data we wrote */
  return compress_len - strm->avail_in;
}

static int mailstream_low_compress_close(mailstream_low * s)
{
  compress_data * data = s->data;
  return data->ms->driver->mailstream_close(data->ms);
}

static int mailstream_low_compress_get_fd(mailstream_low * s)
{
  compress_data * data = s->data;
  return data->ms->driver->mailstream_get_fd(data->ms);
}

static struct mailstream_cancel * mailstream_low_compress_get_cancel(mailstream_low * s)
{
  compress_data * data = s->data;
  return data->ms->driver->mailstream_get_cancel(data->ms);
}

static void mailstream_low_compress_free(mailstream_low * s)
{
  compress_data * data = s->data;
  data->ms->driver->mailstream_free(data->ms);
  if (data->compress_stream) {
    deflateEnd(data->compress_stream);
    free(data->compress_stream);
  }
  if (data->decompress_stream) {
    inflateEnd(data->decompress_stream);
    free(data->decompress_stream);
  }
  free(data);
  free(s);
}

static void mailstream_low_compress_cancel(mailstream_low * s)
{
  compress_data * data = s->data;
  data->ms->driver->mailstream_cancel(data->ms);
}

static carray * mailstream_low_compress_get_certificate_chain(mailstream_low * s)
{
  compress_data * data = s->data;
  return data->ms->driver->mailstream_get_certificate_chain(data->ms);
}

int mailstream_low_compress_wait_idle(mailstream_low * low,
                                      struct mailstream_cancel * idle,
                                      int max_idle_delay)
{
  compress_data * data = low->data;
  return mailstream_low_wait_idle(data->ms, idle, max_idle_delay);
}

#endif

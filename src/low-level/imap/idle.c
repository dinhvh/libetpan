/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2014 - DINH Viet Hoa
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
#	include <config.h>
#endif

#include "idle.h"

#ifdef WIN32
#	include <win_etpan.h>
#endif

#include <stdlib.h>
#include <time.h>

#include "mailimap_sender.h"
#include "mailimap_parser.h"
#include "mailimap.h"

static int mailimap_idle_send(mailimap * session)
{
  int r;
  
  r = mailimap_token_send(session->imap_stream, "IDLE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

static int mailimap_done_send(mailimap * session)
{
  int r;
  
  r = mailimap_token_send(session->imap_stream, "DONE");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

static int mailimap_idle_wait_continuation(mailimap * session)
{
  int r;
  size_t indx;
  struct mailimap_continue_req * cont_req;
  struct mailimap_response_data * resp_data;
  struct mailimap_response * response;
  struct mailimap_parser_context * parser_ctx;

  parser_ctx = mailimap_parser_context_new(session);
  if (parser_ctx == NULL)
    return MAILIMAP_ERROR_MEMORY;

  while (1) {
    indx = 0;
    r = mailimap_continue_req_parse(session->imap_stream,
        session->imap_stream_buffer, parser_ctx,
        &indx, &cont_req,
        session->imap_progr_rate, session->imap_progr_fun);

    if (r == MAILIMAP_NO_ERROR) {
      mailimap_continue_req_free(cont_req);
      mailimap_parser_context_free(parser_ctx);
      return MAILIMAP_NO_ERROR;
    }
    if (r != MAILIMAP_ERROR_PARSE) {
      mailimap_parser_context_free(parser_ctx);
      return r;
    }

    indx = 0;
    r = mailimap_response_data_parse(session->imap_stream,
        session->imap_stream_buffer, parser_ctx,
        &indx, &resp_data,
        session->imap_progr_rate, session->imap_progr_fun);

    if (r == MAILIMAP_NO_ERROR) {
      mailimap_response_data_free(resp_data);
      if (mailimap_read_line(session) == NULL) {
        mailimap_parser_context_free(parser_ctx);
        return MAILIMAP_ERROR_STREAM;
      }
      continue;
    }
    if (r != MAILIMAP_ERROR_PARSE) {
      mailimap_parser_context_free(parser_ctx);
      return r;
    }

    mailimap_parser_context_free(parser_ctx);

    r = mailimap_parse_response(session, &response);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    mailimap_response_free(response);

    return MAILIMAP_ERROR_PARSE;
  }
}

LIBETPAN_EXPORT
int mailimap_idle(mailimap * session)
{
  int r;
  
  session->imap_selection_info->sel_has_exists = 0;
  session->imap_selection_info->sel_has_recent = 0;
  session->imap_idle_timestamp = time(NULL);
  
  r = mailimap_send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;
  
  r = mailimap_idle_send(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;
  
  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;
  
  if (mailimap_read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;

  return mailimap_idle_wait_continuation(session);
}

LIBETPAN_EXPORT
int mailimap_idle_done(mailimap * session)
{
  int r;
  int error_code;
  struct mailimap_response * response;
  
  r = mailimap_done_send(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;
  
  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;
  
  if (mailimap_read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;
  
  r = mailimap_parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_EXTENSION;
  }
}

LIBETPAN_EXPORT
int mailimap_idle_get_fd(mailimap * session)
{
  mailstream_low * low;
  
  low = mailstream_get_low(session->imap_stream);
  return mailstream_low_get_fd(low);
}

LIBETPAN_EXPORT
void mailimap_idle_set_delay(mailimap * session, long delay)
{
  session->imap_idle_maxdelay = delay;
}

LIBETPAN_EXPORT
long mailimap_idle_get_done_delay(mailimap * session)
{
  time_t current_time;
  time_t next_date;
  
  current_time = time(NULL);
  next_date = session->imap_idle_timestamp + session->imap_idle_maxdelay;
  
  if (current_time >= next_date)
    return 0;
  
  return next_date - current_time;
}

LIBETPAN_EXPORT
int mailimap_has_idle(mailimap * session)
{
  return mailimap_has_extension(session, "IDLE");
}

/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
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

/*
 * $Id: newsnntp_socket.c,v 1.15 2009/12/19 00:57:31 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "newsnntp_socket.h"

#include "newsnntp.h"

#include "mailstream_cfstream.h"
#include "connect.h"

#ifdef HAVE_NETINET_IN_H
#	include <netinet/in.h>
#endif
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#define DEFAULT_NNTP_PORT 119
#define SERVICE_NAME_NNTP "nntp"
#define SERVICE_TYPE_TCP "tcp"

static int newsnntp_cfsocket_connect(newsnntp * f, const char * server, uint16_t port);

int newsnntp_socket_connect(newsnntp * f, const char * server, uint16_t port)
{
  int s;
  mailstream * stream;

#if HAVE_CFNETWORK
  if (mailstream_cfstream_enabled) {
    return newsnntp_cfsocket_connect(f, server, port);
  }
#endif
  
  if (port == 0) {
    port = mail_get_service_port(SERVICE_NAME_NNTP, SERVICE_TYPE_TCP);
    if (port == 0)
      port = DEFAULT_NNTP_PORT;
  }

  /* Connection */

  s = mail_tcp_connect_timeout(server, port, f->nntp_timeout);
  if (s == -1)
    return NEWSNNTP_ERROR_CONNECTION_REFUSED;

  stream = mailstream_socket_open_timeout(s, f->nntp_timeout);
  if (stream == NULL) {
#ifdef WIN32
    closesocket(s);
#else
    close(s);
#endif
		
    return NEWSNNTP_ERROR_MEMORY;
  }

  return newsnntp_connect(f, stream);
}

static int newsnntp_cfsocket_connect(newsnntp * f, const char * server, uint16_t port)
{
  mailstream * stream;
  
  stream = mailstream_cfstream_open_timeout(server, port, f->nntp_timeout);
  if (stream == NULL) {
    return NEWSNNTP_ERROR_STREAM;
  }
  
  return newsnntp_connect(f, stream);
}

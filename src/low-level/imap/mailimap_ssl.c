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
 * $Id: mailimap_ssl.c,v 1.17 2009/12/19 00:57:31 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimap_ssl.h"

#include "mailimap.h"

#include "connect.h"

#include <stdlib.h>
#ifdef HAVE_NETINET_IN_H
#	include <netinet/in.h>
#endif
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#include "mailstream_cfstream.h"

#define DEFAULT_IMAPS_PORT 993
#define SERVICE_NAME_IMAPS "imaps"
#define SERVICE_TYPE_TCP "tcp"

int mailimap_ssl_connect_with_callback(mailimap * f, const char * server, uint16_t port,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
  return mailimap_ssl_connect_voip_with_callback(f, server, port, mailstream_cfstream_voip_enabled, callback, data);
}

int mailimap_ssl_connect_voip_with_callback(mailimap * f, const char * server, uint16_t port, int voip_enabled,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
  enum mailstream_ssl_connect_error connect_error;
  mailstream * stream;

  if (port == 0) {
    port = mail_get_service_port(SERVICE_NAME_IMAPS, SERVICE_TYPE_TCP);
    if (port == 0)
      port = DEFAULT_IMAPS_PORT;
  }

  stream = mailstream_ssl_connect_voip_timeout(server, port, f->imap_timeout,
      voip_enabled, callback, data, &connect_error);
  if (stream == NULL) {
    if (connect_error == MAILSTREAM_SSL_CONNECT_ERROR_CONNECTION_REFUSED)
      return MAILIMAP_ERROR_CONNECTION_REFUSED;
    return MAILIMAP_ERROR_SSL;
  }

  return mailimap_connect(f, stream);
}

int mailimap_ssl_connect(mailimap * f, const char * server, uint16_t port)
{
  return mailimap_ssl_connect_voip(f, server, port, mailstream_cfstream_voip_enabled);
}

int mailimap_ssl_connect_voip(mailimap * f, const char * server, uint16_t port, int voip_enabled)
{
  return mailimap_ssl_connect_voip_with_callback(f, server, port, voip_enabled,
      NULL, NULL);
}

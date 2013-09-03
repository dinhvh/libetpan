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
 * $Id: mailstream_types.h,v 1.17 2011/04/19 12:22:17 hoa Exp $
 */

#ifndef MAILSTREAM_TYPES_H

#define MAILSTREAM_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#define LIBETPAN_MAILSTREAM_DEBUG
#ifndef LIBETPAN_CONFIG_H
#	include <libetpan/libetpan-config.h>
#endif

struct _mailstream;

typedef struct _mailstream mailstream;

struct _mailstream_low;

typedef struct _mailstream_low mailstream_low;

struct _mailstream {
  size_t buffer_max_size;

  char * write_buffer;
  size_t write_buffer_len;

  char * read_buffer;
  size_t read_buffer_len;

  mailstream_low * low;
  
  struct mailstream_cancel * idle;
  int idling;
};

struct mailstream_low_driver {
  ssize_t (* mailstream_read)(mailstream_low *, void *, size_t);
  ssize_t (* mailstream_write)(mailstream_low *, const void *, size_t);
  int (* mailstream_close)(mailstream_low *);
  int (* mailstream_get_fd)(mailstream_low *);
  void (* mailstream_free)(mailstream_low *);
  void (* mailstream_cancel)(mailstream_low *);
  struct mailstream_cancel * (* mailstream_get_cancel)(mailstream_low *);
};

typedef struct mailstream_low_driver mailstream_low_driver;

struct _mailstream_low {
  void * data;
  mailstream_low_driver * driver;
  int privacy;
	char * identifier;
};

typedef void progress_function(size_t current, size_t maximum);

typedef void mailprogress_function(size_t current, size_t maximum, void * context);

enum {
  MAILSTREAM_IDLE_ERROR,
  MAILSTREAM_IDLE_INTERRUPTED,
  MAILSTREAM_IDLE_HASDATA,
  MAILSTREAM_IDLE_TIMEOUT,
  MAILSTREAM_IDLE_CANCELLED
};
  
#ifdef __cplusplus
}
#endif

#endif

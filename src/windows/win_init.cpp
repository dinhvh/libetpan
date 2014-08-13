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
/*
Special class, initializers for windows
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <winsock2.h>

#ifdef _MSC_VER
#include "mailstream_ssl_private.h"
#include "mmapstring_private.h"
#include "mailsasl_private.h"
#endif


class win_init {
  public:
	win_init() { 

		wsocket_init();

#ifdef _MSC_VER
		/* Initialize Mutexes */
		mmapstring_init_lock();

#ifdef USE_SSL
		mailstream_ssl_init_lock();
#endif

#ifdef USE_SASL
        mailsasl_init_lock();
#endif
#endif

	}
	~win_init() {
		WSACleanup();
	}

  private:
    WSADATA winsockData;

	void wsocket_init() {
	    int success = WSAStartup((WORD)0x0101, &winsockData);
		if (success != 0)
		{
			throw "Cannot startup windows sockets api.";
		}
	}

};

/* Initialise  */
static win_init windows_startup;


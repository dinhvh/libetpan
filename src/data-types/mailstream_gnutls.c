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
 * $Id: mailstream_ssl.c,v 1.77 2011/08/30 19:42:16 colinleroy Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifdef HAVE_GNUTLS
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(hidden)
#endif

#include "mailstream_ssl.h"
#include "mailstream_ssl_private.h"

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#	include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#	include <string.h>
#endif
#include <fcntl.h>

/*
  these 3 headers MUST be included before <sys/select.h>
  to insure compatibility with Mac OS X (this is true for 10.2)
*/
#ifdef WIN32
#	include <win_etpan.h>
#else
#	include <sys/time.h>
#	include <sys/types.h>
#   if USE_POLL
#       ifdef HAVE_SYS_POLL_H
#	        include <sys/poll.h>
#       endif
#   else 
#       ifdef HAVE_SELECT_H
#	        include <sys/select.h>
#       endif
#   endif
#endif

/* mailstream_low, ssl */

#  include <errno.h>
#  include <gnutls/gnutls.h>
#  include <gnutls/x509.h>
# ifdef LIBETPAN_REENTRANT
#	 if HAVE_PTHREAD_H
#	  include <pthread.h>
#  elif defined(WIN32)
    void mailprivacy_gnupg_init_lock();
    void mailprivacy_smime_init_lock();
#  endif
#	endif

#include "mmapstring.h"
#include "mailstream_cancel.h"

struct mailstream_ssl_context
{
  int fd;
  gnutls_session_t session;
  gnutls_x509_crt_t client_x509;
  gnutls_x509_privkey_t client_pkey;
  gnutls_certificate_credentials_t gnutls_credentials;
};

struct mailstream_ssl_server_name_callback_data {
  const char * server_name;
  void (* callback)(struct mailstream_ssl_context * ssl_context, void * data);
  void * callback_data;
};

static int mailstream_gnutls_ssl_set_server_name(
    struct mailstream_ssl_context * ssl_context, const char * hostname);
static mailstream_low * mailstream_gnutls_low_ssl_open_timeout(
    int fd, time_t timeout);
static mailstream_low * mailstream_gnutls_low_tls_open_timeout(
    int fd, time_t timeout);
static mailstream * mailstream_gnutls_ssl_open_timeout(
    int fd, time_t timeout);
static mailstream * mailstream_gnutls_ssl_open_with_callback_timeout(
    int fd, time_t timeout,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data);
static mailstream_low * mailstream_gnutls_low_ssl_open_with_callback_timeout(
    int fd, time_t timeout,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data);
static mailstream_low * mailstream_gnutls_low_tls_open_with_callback_timeout(
    int fd, time_t timeout,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data);

struct mailstream_ssl_data {
  int fd;
  gnutls_session_t session;
  gnutls_certificate_credentials_t xcred;
  struct mailstream_cancel * cancel;
#ifdef WIN32
  HANDLE read_event;
  HANDLE write_event;
#endif
};

#ifdef LIBETPAN_REENTRANT
#	if HAVE_PTHREAD_H
#		define MUTEX_LOCK(x) pthread_mutex_lock(x)
#		define MUTEX_UNLOCK(x) pthread_mutex_unlock(x)
		static pthread_mutex_t ssl_lock = PTHREAD_MUTEX_INITIALIZER;
#	elif (defined WIN32)
#		define MUTEX_LOCK(x) EnterCriticalSection(x);
#		define MUTEX_UNLOCK(x) LeaveCriticalSection(x);
		static CRITICAL_SECTION ssl_lock;
#	else
#		error "What are your threads?"
#	endif
#else
#	define MUTEX_LOCK(x)
#	define MUTEX_UNLOCK(x)
#endif
void mailstream_gnutls_ssl_init_lock(void)
{
#if !defined (HAVE_PTHREAD_H) && defined (WIN32) && defined(HAVE_GNUTLS)
  static long volatile mailstream_ssl_init_lock_done = 0;
  if (InterlockedExchange(&mailstream_ssl_init_lock_done, 1) == 0) {
    InitializeCriticalSection(&ssl_lock);
  }
#endif
}

void mailstream_gnutls_ssl_uninit_lock(void)
{
#if !defined (HAVE_PTHREAD_H) && defined (WIN32) && defined(HAVE_GNUTLS)
	static long volatile mailstream_ssl_init_lock_done = 0;
	if (InterlockedExchange(&mailstream_ssl_init_lock_done, 1) == 0) {
		DeleteCriticalSection(&ssl_lock);
	}
#endif
}

void mailstream_gnutls_ssl_init_not_required(void)
{
}

static inline void mailstream_ssl_init(void)
{
  mailstream_gnutls_ssl_init_lock();
  MUTEX_LOCK(&ssl_lock);
  gnutls_global_init();
  MUTEX_UNLOCK(&ssl_lock);
}

static inline int mailstream_prepare_fd(int fd)
{
#ifndef WIN32
  int fd_flags;
  int r;
  
  fd_flags = fcntl(fd, F_GETFL, 0);
  fd_flags |= O_NDELAY;
  r = fcntl(fd, F_SETFL, fd_flags);
  if (r < 0)
    return -1;
#endif
  
  return 0;
}

static int wait_SSL_connect(int s, int want_read, time_t timeout_seconds)
{
  struct timeval timeout;
  int r;
#if defined(WIN32) || !USE_POLL
  fd_set fds;
#else
  struct pollfd pfd;
#endif // WIN32

  if (timeout_seconds == 0) {
    timeout = mailstream_network_delay;
  }
  else {
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
  }
#if defined(WIN32) || !USE_POLL
  FD_ZERO(&fds);
  FD_SET(s, &fds);
  /* TODO: how to cancel this ? */
  if (want_read)
    r = select(s + 1, &fds, NULL, NULL, &timeout);
  else
    r = select(s + 1, NULL, &fds, NULL, &timeout);
  if (r <= 0) {
    return -1;
  }
  if (!FD_ISSET(s, &fds)) {
    /* though, it's strange */
    return -1;
  }
#else
  pfd.fd = s;
  if (want_read) {
    pfd.events = POLLIN;
  }
  else {
    pfd.events = POLLOUT;
  }
  r = poll(&pfd, 1, timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
  if (r <= 0) {
    return -1;
  }

  if ((pfd.revents & pfd.events) != pfd.events) {
    return -1;
  }
#endif
  
  return 0;
}

static int mailstream_low_ssl_close(mailstream_low * s);
static ssize_t mailstream_low_ssl_read(mailstream_low * s,
				       void * buf, size_t count);
static ssize_t mailstream_low_ssl_write(mailstream_low * s,
					const void * buf, size_t count);
static void mailstream_low_ssl_free(mailstream_low * s);
static int mailstream_low_ssl_get_fd(mailstream_low * s);
static void mailstream_low_ssl_cancel(mailstream_low * s);
static struct mailstream_cancel * mailstream_low_ssl_get_cancel(mailstream_low * s);
static carray * mailstream_gnutls_low_ssl_get_certificate_chain(mailstream_low * s);

static void mailstream_ssl_server_name_callback(struct mailstream_ssl_context * ssl_context, void * data)
{
  struct mailstream_ssl_server_name_callback_data * callback_data;

  callback_data = data;
  if (callback_data->server_name != NULL)
    mailstream_gnutls_ssl_set_server_name(ssl_context, callback_data->server_name);

  if (callback_data->callback != NULL)
    callback_data->callback(ssl_context, callback_data->callback_data);
}

static mailstream_low_driver local_mailstream_ssl_driver = {
  /* mailstream_read */ mailstream_low_ssl_read,
  /* mailstream_write */ mailstream_low_ssl_write,
  /* mailstream_close */ mailstream_low_ssl_close,
  /* mailstream_get_fd */ mailstream_low_ssl_get_fd,
  /* mailstream_free */ mailstream_low_ssl_free,
  /* mailstream_cancel */ mailstream_low_ssl_cancel,
  /* mailstream_get_cancel */ mailstream_low_ssl_get_cancel,
  /* mailstream_get_certificate_chain */ mailstream_gnutls_low_ssl_get_certificate_chain,
  /* mailstream_setup_idle */ NULL,
  /* mailstream_unsetup_idle */ NULL,
  /* mailstream_interrupt_idle */ NULL,
};

mailstream_low_driver * mailstream_gnutls_ssl_driver = &local_mailstream_ssl_driver;

/* file descriptor must be given in (default) blocking-mode */


static struct mailstream_ssl_context * mailstream_ssl_context_new(gnutls_session_t session, int fd);
static void mailstream_ssl_context_free(struct mailstream_ssl_context * ssl_ctx);

#if GNUTLS_VERSION_NUMBER <= 0x020c00
static int mailstream_gnutls_client_cert_cb(gnutls_session_t session,
                               const gnutls_datum_t *req_ca_rdn, int nreqs,
                               const gnutls_pk_algorithm_t *sign_algos,
                               int sign_algos_length, gnutls_retr_st *st)
#else
static int mailstream_gnutls_client_cert_cb(gnutls_session_t session,
                               const gnutls_datum_t *req_ca_rdn, int nreqs,
                               const gnutls_pk_algorithm_t *sign_algos,
                               int sign_algos_length, gnutls_retr2_st *st)
#endif
{
	struct mailstream_ssl_context * ssl_context = (struct mailstream_ssl_context *)gnutls_session_get_ptr(session);
	gnutls_certificate_type_t type = gnutls_certificate_type_get(session);

	st->ncerts = 0;

	if (ssl_context == NULL)
		return 0;

	if (type == GNUTLS_CRT_X509 && ssl_context->client_x509 && ssl_context->client_pkey) {
		st->ncerts = 1;
#if GNUTLS_VERSION_NUMBER <= 0x020c00
		st->type = type;
#else
		st->cert_type = type;
		st->key_type = GNUTLS_PRIVKEY_X509;
#endif
		st->cert.x509 = &(ssl_context->client_x509);
		st->key.x509 = ssl_context->client_pkey;
		st->deinit_all = 0;
	}
	return 0;
}

static struct mailstream_ssl_data * ssl_data_new(int fd, time_t timeout,
  void (* callback)(struct mailstream_ssl_context * ssl_context, void * cb_data), void * cb_data)
{
  struct mailstream_ssl_data * ssl_data;
  gnutls_session_t session;
  struct mailstream_cancel * cancel;
  gnutls_certificate_credentials_t xcred;
  int r;
  struct mailstream_ssl_context * ssl_context = NULL;
  unsigned int timeout_value;
  
  mailstream_ssl_init();
  
  if (gnutls_certificate_allocate_credentials (&xcred) != 0)
    return NULL;

  r = gnutls_init(&session, GNUTLS_CLIENT);
  if (session == NULL || r != 0)
    return NULL;
  
  if (callback != NULL) {
    ssl_context = mailstream_ssl_context_new(session, fd);
    callback(ssl_context, cb_data);
  }
  
  gnutls_session_set_ptr(session, ssl_context);
  gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);
#if GNUTLS_VERSION_NUMBER <= 0x020c00
  gnutls_certificate_client_set_retrieve_function(xcred, mailstream_gnutls_client_cert_cb);
#else
  gnutls_certificate_set_retrieve_function(xcred, mailstream_gnutls_client_cert_cb);
#endif
  gnutls_set_default_priority(session);
  gnutls_priority_set_direct(session, "NORMAL", NULL);

  gnutls_record_disable_padding(session);
  gnutls_dh_set_prime_bits(session, 512);

  gnutls_transport_set_int(session, fd);

  /* lower limits on server key length restriction */
  gnutls_dh_set_prime_bits(session, 512);
  
  if (timeout == 0) {
		timeout_value = mailstream_network_delay.tv_sec * 1000 + mailstream_network_delay.tv_usec / 1000;
  }
  else {
		timeout_value = timeout * 1000;
  }
#if GNUTLS_VERSION_NUMBER >= 0x030100
	gnutls_handshake_set_timeout(session, timeout_value);
#endif

  do {
    r = gnutls_handshake(session);
  } while (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED);

  if (r < 0) {
    gnutls_perror(r);
    goto free_ssl_conn;
  }
  
  cancel = mailstream_cancel_new();
  if (cancel == NULL)
    goto free_ssl_conn;
  
  r = mailstream_prepare_fd(fd);
  if (r < 0)
    goto free_cancel;
  
  ssl_data = malloc(sizeof(* ssl_data));
  if (ssl_data == NULL)
    goto err;
  
  ssl_data->fd = fd;
  ssl_data->session = session;
  ssl_data->xcred = xcred;
  ssl_data->cancel = cancel;
#ifdef WIN32
  ssl_data->read_event = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (ssl_data->read_event == NULL)
    goto free_ssl_data;

  ssl_data->write_event = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (ssl_data->write_event == NULL)
    goto close_read_event;
#endif
  
  mailstream_ssl_context_free(ssl_context);

  return ssl_data;
  
#ifdef WIN32
 close_read_event:
  CloseHandle(ssl_data->read_event);
 free_ssl_data:
  free(ssl_data);
#endif
 free_cancel:
  mailstream_cancel_free(cancel);
 free_ssl_conn:
  gnutls_certificate_free_credentials(xcred);
  mailstream_ssl_context_free(ssl_context);
  gnutls_deinit(session);
 err:
  return NULL;
}

static void  ssl_data_free(struct mailstream_ssl_data * ssl_data)
{
  mailstream_cancel_free(ssl_data->cancel);
#ifdef WIN32
  CloseHandle(ssl_data->read_event);
  CloseHandle(ssl_data->write_event);
#endif
  free(ssl_data);
}

static void  ssl_data_close(struct mailstream_ssl_data * ssl_data)
{
  gnutls_certificate_free_credentials(ssl_data->xcred);
  gnutls_deinit(ssl_data->session);

  MUTEX_LOCK(&ssl_lock);
  gnutls_global_deinit();
  MUTEX_UNLOCK(&ssl_lock);

  ssl_data->session = NULL;
#ifdef WIN32
  closesocket(ssl_data->fd);
#else
  close(ssl_data->fd);
#endif
  ssl_data->fd = -1;
}


static mailstream_low * mailstream_low_ssl_open_full(int fd, int starttls, time_t timeout,
  void (* callback)(struct mailstream_ssl_context * ssl_context, void * cb_data), void * cb_data)
{
  mailstream_low * s;
  struct mailstream_ssl_data * ssl_data;

  ssl_data = ssl_data_new(fd, timeout, callback, cb_data);

  if (ssl_data == NULL)
    goto err;

  s = mailstream_low_new(ssl_data, mailstream_gnutls_ssl_driver);
  if (s == NULL)
    goto free_ssl_data;
	mailstream_low_set_timeout(s, timeout);

  return s;

 free_ssl_data:
  ssl_data_free(ssl_data);
 err:
  return NULL;
}

mailstream_low * mailstream_gnutls_low_ssl_open(int fd)
{
	return mailstream_gnutls_low_ssl_open_timeout(fd, 0);
}

mailstream_low * mailstream_gnutls_low_tls_open(int fd)
{
	return mailstream_gnutls_low_tls_open_timeout(fd, 0);
}

mailstream_low * mailstream_gnutls_low_ssl_open_timeout(int fd, time_t timeout)
{
  return mailstream_low_ssl_open_full(fd, 0, timeout, NULL, NULL);
}

mailstream_low * mailstream_gnutls_low_tls_open_timeout(int fd, time_t timeout)
{
  return mailstream_low_ssl_open_full(fd, 1, timeout, NULL, NULL);
}

static int mailstream_low_ssl_close(mailstream_low * s)
{
  struct mailstream_ssl_data * ssl_data;

  ssl_data = (struct mailstream_ssl_data *) s->data;
  ssl_data_close(ssl_data);

  return 0;
}

static void mailstream_low_ssl_free(mailstream_low * s)
{
  struct mailstream_ssl_data * ssl_data;

  ssl_data = (struct mailstream_ssl_data *) s->data;
  ssl_data_free(ssl_data);
  s->data = NULL;

  free(s);
}

static int mailstream_low_ssl_get_fd(mailstream_low * s)
{
  struct mailstream_ssl_data * ssl_data;

  ssl_data = (struct mailstream_ssl_data *) s->data;
  return ssl_data->fd;
}

static int wait_read(mailstream_low * s)
{
  struct timeval timeout;
  int cancellation_fd;
  struct mailstream_ssl_data * ssl_data;
  int r;
  int cancelled;
#if defined(WIN32)
  fd_set fds_read;
  HANDLE event;
#elif USE_POLL
  struct pollfd pfd[2];
#else
  fd_set fds_read;
  int max_fd;
#endif

  ssl_data = (struct mailstream_ssl_data *) s->data;
  if (s->timeout == 0) {
    timeout = mailstream_network_delay;
  }
  else {
		timeout.tv_sec = s->timeout;
    timeout.tv_usec = 0;
  }
  
  if (gnutls_record_check_pending(ssl_data->session) != 0)
    return 0;

  cancellation_fd = mailstream_cancel_get_fd(ssl_data->cancel);
#if defined(WIN32)
  FD_ZERO(&fds_read);
  FD_SET(cancellation_fd, &fds_read);
  event = ssl_data->read_event;
  ResetEvent(event);
  WSAEventSelect(ssl_data->fd, event, FD_READ | FD_CLOSE);
  FD_SET(event, &fds_read);
  r = WaitForMultipleObjects(fds_read.fd_count, fds_read.fd_array, FALSE, timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
  if (r < WAIT_OBJECT_0 || r >= WAIT_OBJECT_0 + fds_read.fd_count) {
    WSAEventSelect(ssl_data->fd, event, 0);
    return -1;
  }
  
  cancelled = (fds_read.fd_array[r - WAIT_OBJECT_0] == cancellation_fd);
  if (fds_read.fd_array[r - WAIT_OBJECT_0] == event) {
    WSANETWORKEVENTS events;
    if (WSAEnumNetworkEvents(ssl_data->fd, event, &events) == SOCKET_ERROR) {
      WSAEventSelect(ssl_data->fd, event, 0);
      return -1;
    }
  }
  WSAEventSelect(ssl_data->fd, event, 0);
#elif USE_POLL
  pfd[0].fd = ssl_data->fd;
  pfd[0].events = POLLIN;
  pfd[0].revents = 0;

  pfd[1].fd = cancellation_fd;
  pfd[1].events = POLLIN;
  pfd[1].revents = 0;

  r = poll(&pfd[0], 2, timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
  if (r <= 0)
    return -1;
  
  cancelled = pfd[1].revents & POLLIN;
#else
  FD_ZERO(&fds_read);
  FD_SET(cancellation_fd, &fds_read);
  FD_SET(ssl_data->fd, &fds_read);
  max_fd = cancellation_fd > ssl_data->fd ? cancellation_fd : ssl_data->fd;
  r = select(max_fd + 1, &fds_read, NULL, NULL, &timeout);
  if (r <= 0)
      return -1;
  cancelled = FD_ISSET(cancellation_fd, &fds_read);
#endif
  if (cancelled) {
    /* cancelled */
    mailstream_cancel_ack(ssl_data->cancel);
    return -1;
  }
  
  return 0;
}

static ssize_t mailstream_low_ssl_read(mailstream_low * s,
				       void * buf, size_t count)
{
  struct mailstream_ssl_data * ssl_data;
  int r;

  ssl_data = (struct mailstream_ssl_data *) s->data;
  if (mailstream_cancel_cancelled(ssl_data->cancel))
    return -1;
  
  while (1) {
    r = gnutls_record_recv(ssl_data->session, buf, count);
    if (r > 0)
      return r;
    
    switch (r) {
    case 0: /* closed connection */
      return -1;
    
    case GNUTLS_E_REHANDSHAKE:
      do {
         r = gnutls_handshake(ssl_data->session); 
      } while (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED);
      break; /* re-receive */
    case GNUTLS_E_AGAIN:
    case GNUTLS_E_INTERRUPTED:
      r = wait_read(s);
      if (r < 0)
        return r;
      break;
      
    default:
      return -1;
    }
  }
}

static int wait_write(mailstream_low * s)
{
  struct timeval timeout;
  int r;
  int cancellation_fd;
  struct mailstream_ssl_data * ssl_data;
  int cancelled;
  int write_enabled;
#if defined(WIN32)
  fd_set fds_read;
  fd_set fds_write;
  HANDLE event;
#elif USE_POLL
  struct pollfd pfd[2];
#else
  fd_set fds_read;
  fd_set fds_write;
  int max_fd;
#endif
  
  ssl_data = (struct mailstream_ssl_data *) s->data;
  if (mailstream_cancel_cancelled(ssl_data->cancel))
    return -1;
 
  if (s->timeout == 0) {
    timeout = mailstream_network_delay;
  }
  else {
		timeout.tv_sec = s->timeout;
    timeout.tv_usec = 0;
  }
  
  cancellation_fd = mailstream_cancel_get_fd(ssl_data->cancel);
#if defined(WIN32)
  FD_ZERO(&fds_read);
  FD_ZERO(&fds_write);
  FD_SET(cancellation_fd, &fds_read);
  event = ssl_data->write_event;
  ResetEvent(event);
  WSAEventSelect(ssl_data->fd, event, FD_WRITE | FD_CLOSE);
  FD_SET(event, &fds_read);
  r = WaitForMultipleObjects(fds_read.fd_count, fds_read.fd_array, FALSE, timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
  if (r < WAIT_OBJECT_0 || r >= WAIT_OBJECT_0 + fds_read.fd_count) {
		WSAEventSelect(ssl_data->fd, event, 0);
    return -1;
	}
  
  cancelled = (fds_read.fd_array[r - WAIT_OBJECT_0] == cancellation_fd) /* SEB 20070709 */;
  write_enabled = (fds_read.fd_array[r - WAIT_OBJECT_0] == event);
  if (write_enabled) {
    WSANETWORKEVENTS events;
    if (WSAEnumNetworkEvents(ssl_data->fd, event, &events) == SOCKET_ERROR) {
      WSAEventSelect(ssl_data->fd, event, 0);
      return -1;
    }
    write_enabled = (events.lNetworkEvents & (FD_WRITE | FD_CLOSE)) != 0;
  }
	WSAEventSelect(ssl_data->fd, event, 0);
#elif USE_POLL
  pfd[0].fd = ssl_data->fd;
  pfd[0].events = POLLOUT;
  pfd[0].revents = 0;

  pfd[1].fd = cancellation_fd;
  pfd[1].events = POLLIN;
  pfd[1].revents = 0;

  r = poll(&pfd[0], 2, timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
  if (r <= 0)
    return -1;
 
  cancelled = pfd[1].revents & POLLIN;
  write_enabled = pfd[0].revents & POLLOUT;
#else
  FD_ZERO(&fds_read);
  FD_ZERO(&fds_write);
  FD_SET(cancellation_fd, &fds_read);
  FD_SET(ssl_data->fd, &fds_write);
  
  max_fd = cancellation_fd > ssl_data->fd ? cancellation_fd : ssl_data->fd;
  r = select(max_fd + 1, &fds_read, &fds_write, NULL, &timeout);
  if (r <= 0)
    return -1;

  cancelled = FD_ISSET(cancellation_fd, &fds_read);
  write_enabled = FD_ISSET(ssl_data->fd, &fds_write);
#endif
  
  if (cancelled) {
    /* cancelled */
    mailstream_cancel_ack(ssl_data->cancel);
    return -1;
  }
  
  if (!write_enabled)
    return 0;
  
  return 1;
}

static ssize_t mailstream_low_ssl_write(mailstream_low * s,
					const void * buf, size_t count)
{
  struct mailstream_ssl_data * ssl_data;
  int r;
  
  ssl_data = (struct mailstream_ssl_data *) s->data;
  r = wait_write(s);
  if (r <= 0)
    return r;
  
  r = gnutls_record_send(ssl_data->session, buf, count);
  if (r > 0)
    return r;
  
  switch (r) {
  case 0:
    return -1;
    
  case GNUTLS_E_AGAIN:
  case GNUTLS_E_INTERRUPTED:
    return 0;
    
  default:
    return r;
  }
}

/* mailstream */

mailstream * mailstream_gnutls_ssl_open(int fd)
{
  return mailstream_gnutls_ssl_open_timeout(fd, 0);
}

mailstream * mailstream_gnutls_ssl_open_timeout(int fd, time_t timeout)
{
  return mailstream_gnutls_ssl_open_with_callback_timeout(fd, timeout, NULL, NULL);
}

mailstream * mailstream_gnutls_ssl_open_with_callback(int fd,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
	return mailstream_gnutls_ssl_open_with_callback_timeout(fd, 0, callback, data);
}

mailstream * mailstream_gnutls_ssl_open_with_callback_timeout(int fd, time_t timeout,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
  mailstream_low * low;
  mailstream * s;

  low = mailstream_gnutls_low_ssl_open_with_callback_timeout(fd, timeout, callback, data);
  if (low == NULL)
    goto err;

  s = mailstream_new(low, 8192);
  if (s == NULL)
    goto free_low;

  return s;

 free_low:
  mailstream_low_close(low);
 err:
  return NULL;
}

mailstream * mailstream_gnutls_ssl_open_with_server_name_callback_timeout(int fd, time_t timeout,
    const char * server_name,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
  struct mailstream_ssl_server_name_callback_data callback_data;

  callback_data.server_name = server_name;
  callback_data.callback = callback;
  callback_data.callback_data = data;

  return mailstream_gnutls_ssl_open_with_callback_timeout(fd, timeout,
      mailstream_ssl_server_name_callback, &callback_data);
}

ssize_t mailstream_gnutls_ssl_get_certificate(mailstream *stream, unsigned char **cert_DER)
{
  struct mailstream_ssl_data *data = NULL;
  ssize_t len = 0;
  gnutls_session_t session = NULL;
  const gnutls_datum_t *raw_cert_list;
  unsigned int raw_cert_list_length;
  gnutls_x509_crt_t cert = NULL;
  size_t cert_size;

  if (cert_DER == NULL || stream == NULL || stream->low == NULL)
    return -1;

  data = stream->low->data;
  if (data == NULL)
    return -1;

  session = data->session;
  raw_cert_list = gnutls_certificate_get_peers(session, &raw_cert_list_length);

  if (raw_cert_list 
  && gnutls_certificate_type_get(session) == GNUTLS_CRT_X509
  &&  gnutls_x509_crt_init(&cert) >= 0
  &&  gnutls_x509_crt_import(cert, &raw_cert_list[0], GNUTLS_X509_FMT_DER) >= 0) {
    cert_size = 0;
    if (gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_DER, NULL, &cert_size) 
        != GNUTLS_E_SHORT_MEMORY_BUFFER)
      return -1;

    *cert_DER = malloc (cert_size);
    if (*cert_DER == NULL)
      return -1;

    if (gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_DER, *cert_DER, &cert_size) < 0)
      return -1;

    len = (ssize_t)cert_size;
    gnutls_x509_crt_deinit(cert);
    
    return len;
  }
  return -1;
}

static void mailstream_low_ssl_cancel(mailstream_low * s)
{
  struct mailstream_ssl_data * data;
  
  data = s->data;
  mailstream_cancel_notify(data->cancel);
}

mailstream_low * mailstream_gnutls_low_ssl_open_with_callback(int fd,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
	return mailstream_gnutls_low_ssl_open_with_callback_timeout(fd, 0, callback, data);
}

mailstream_low * mailstream_gnutls_low_ssl_open_with_callback_timeout(int fd, time_t timeout,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
  return mailstream_low_ssl_open_full(fd, 0, timeout, callback, data);
}

mailstream_low * mailstream_gnutls_low_tls_open_with_callback(int fd,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
  return mailstream_gnutls_low_tls_open_with_callback_timeout(fd, 0, callback, data);
}

mailstream_low * mailstream_gnutls_low_tls_open_with_callback_timeout(int fd, time_t timeout,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
  return mailstream_low_ssl_open_full(fd, 1, timeout, callback, data);
}

mailstream_low * mailstream_gnutls_low_ssl_open_with_server_name_callback_timeout(int fd, time_t timeout,
    const char * server_name,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
  struct mailstream_ssl_server_name_callback_data callback_data;

  callback_data.server_name = server_name;
  callback_data.callback = callback;
  callback_data.callback_data = data;

  return mailstream_gnutls_low_ssl_open_with_callback_timeout(fd, timeout,
      mailstream_ssl_server_name_callback, &callback_data);
}

mailstream_low * mailstream_gnutls_low_tls_open_with_server_name_callback_timeout(int fd, time_t timeout,
    const char * server_name,
    void (* callback)(struct mailstream_ssl_context * ssl_context, void * data), void * data)
{
  struct mailstream_ssl_server_name_callback_data callback_data;

  callback_data.server_name = server_name;
  callback_data.callback = callback;
  callback_data.callback_data = data;

  return mailstream_gnutls_low_tls_open_with_callback_timeout(fd, timeout,
      mailstream_ssl_server_name_callback, &callback_data);
}

int mailstream_gnutls_ssl_set_client_certicate(struct mailstream_ssl_context * ssl_context,
    char * filename)
{
  /* not implemented */
  return -1;
}

LIBETPAN_EXPORT
int mailstream_gnutls_ssl_set_client_certificate_data(struct mailstream_ssl_context * ssl_context,
    unsigned char *x509_der, size_t len)
{
  gnutls_datum_t tmp;
  int r;
  ssl_context->client_x509 = NULL;
  if (len == 0)
    return 0;
  gnutls_x509_crt_init(&(ssl_context->client_x509));
  tmp.data = x509_der;
  tmp.size = len;
  if ((r = gnutls_x509_crt_import(ssl_context->client_x509, &tmp, GNUTLS_X509_FMT_DER)) < 0) {
    gnutls_x509_crt_deinit(ssl_context->client_x509);
    ssl_context->client_x509 = NULL;
    return -1;
  }
  return 0;
  return -1;
}
int mailstream_gnutls_ssl_set_client_private_key_data(struct mailstream_ssl_context * ssl_context,
    unsigned char *pkey_der, size_t len)
{
  gnutls_datum_t tmp;
  int r;
  ssl_context->client_pkey = NULL;
  if (len == 0)
    return 0;
  gnutls_x509_privkey_init(&(ssl_context->client_pkey));
  tmp.data = pkey_der;
  tmp.size = len;
  if ((r = gnutls_x509_privkey_import(ssl_context->client_pkey, &tmp, GNUTLS_X509_FMT_DER)) < 0) {
    gnutls_x509_privkey_deinit(ssl_context->client_pkey);
    ssl_context->client_pkey = NULL;
    return -1;
  }
  return 0;
  return -1;
}

int mailstream_gnutls_ssl_set_server_certicate(struct mailstream_ssl_context * ssl_context, 
    char * CAfile, char * CApath)
{
  /* not implemented */
  return -1;
}

LIBETPAN_EXPORT
int mailstream_gnutls_ssl_set_server_name(struct mailstream_ssl_context * ssl_context,
    const char * hostname)
{
  int r = -1;

  if (hostname != NULL) {
    r = gnutls_server_name_set(ssl_context->session, GNUTLS_NAME_DNS, hostname, strlen(hostname));
  }
  else {
    r = gnutls_server_name_set(ssl_context->session, GNUTLS_NAME_DNS, "", 0U);
  }

  return r;
}

void mailstream_gnutls_ssl_set_server_name_callback(struct mailstream_ssl_context * ssl_context,
    void * data)
{
  mailstream_gnutls_ssl_set_server_name(ssl_context, (const char *) data);
}

static struct mailstream_ssl_context * mailstream_ssl_context_new(gnutls_session_t session, int fd)
{
  struct mailstream_ssl_context * ssl_ctx;
  
  ssl_ctx = malloc(sizeof(* ssl_ctx));
  if (ssl_ctx == NULL)
    return NULL;
  
  ssl_ctx->session = session;
  ssl_ctx->client_x509 = NULL;
  ssl_ctx->client_pkey = NULL;
  ssl_ctx->fd = fd;
  
  return ssl_ctx;
}

static void mailstream_ssl_context_free(struct mailstream_ssl_context * ssl_ctx)
{
  if (ssl_ctx) {
    if (ssl_ctx->client_x509)
      gnutls_x509_crt_deinit(ssl_ctx->client_x509);
    if (ssl_ctx->client_pkey)
      gnutls_x509_privkey_deinit(ssl_ctx->client_pkey);
    free(ssl_ctx);
  }
}

int mailstream_gnutls_ssl_get_fd(struct mailstream_ssl_context * ssl_context)
{
  return ssl_context->fd;
}

static struct mailstream_cancel * mailstream_low_ssl_get_cancel(mailstream_low * s)
{
  struct mailstream_ssl_data * data;
  
  data = s->data;
  return data->cancel;
}

static void mailstream_low_ssl_certificate_chain_free(carray * result)
{
  unsigned int i;

  if (result == NULL)
    return;

  for(i = 0 ; i < carray_count(result) ; i ++)
    mmap_string_free(carray_get(result, i));
  carray_free(result);
}

carray * mailstream_gnutls_low_ssl_get_certificate_chain(mailstream_low * s)
{
  struct mailstream_ssl_data * ssl_data;
  carray * result;
  int skpos;
  gnutls_session_t session = NULL;
  const gnutls_datum_t *raw_cert_list;
  unsigned int raw_cert_list_length;

  ssl_data = (struct mailstream_ssl_data *) s->data;

  session = ssl_data->session;
  raw_cert_list = gnutls_certificate_get_peers(session, &raw_cert_list_length);

  if ((raw_cert_list == NULL) ||
      (gnutls_certificate_type_get(session) != GNUTLS_CRT_X509))
    return NULL;

  result = carray_new(4);
  if (result == NULL)
    return NULL;

  for(skpos = 0 ; skpos < raw_cert_list_length ; skpos ++) {
    gnutls_x509_crt_t cert = NULL;
    size_t cert_size = 0;
    MMAPString * str = NULL;
    unsigned char * p;
    int r;

    r = gnutls_x509_crt_init(&cert);
    if (r < 0)
      goto err;

    r = gnutls_x509_crt_import(cert, &raw_cert_list[skpos], GNUTLS_X509_FMT_DER);
    if (r < 0)
      goto free_cert;

    r = gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_DER, NULL, &cert_size);
    if (r != GNUTLS_E_SHORT_MEMORY_BUFFER)
      goto free_cert;

    str = mmap_string_sized_new(cert_size);
    if (str == NULL)
      goto free_cert;

    p = (unsigned char *) str->str;
    str->len = cert_size;
    r = gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_DER, p, &cert_size);
    if (r < 0) {
      mmap_string_free(str);
      goto free_cert;
    }

    if (carray_add(result, str, NULL) < 0) {
      mmap_string_free(str);
      goto free_cert;
    }

    gnutls_x509_crt_deinit(cert);
    continue;

   free_cert:
    gnutls_x509_crt_deinit(cert);
    goto err;
  }

  return result;

 err:
  mailstream_low_ssl_certificate_chain_free(result);
  return NULL;
}

#include "mailstream_ssl_backend.h"

static mailstream_low * mailstream_gnutls_driver_open_low(int fd,
    int starttls, time_t timeout,
    void (* callback)(void *, void *), void * data)
{
  if (starttls)
    return mailstream_gnutls_low_tls_open_with_callback_timeout(
        fd, timeout, (void (*)(struct mailstream_ssl_context *, void *)) callback,
        data);
  return mailstream_gnutls_low_ssl_open_with_callback_timeout(
      fd, timeout, (void (*)(struct mailstream_ssl_context *, void *)) callback,
      data);
}

static struct mailstream_ssl_backend_driver mailstream_gnutls_driver = {
  MAILSTREAM_SSL_BACKEND_GNUTLS,
  "gnutls",
  NULL,
  mailstream_gnutls_driver_open_low,
  mailstream_gnutls_ssl_get_certificate,
  (int (*)(void *, char *)) mailstream_gnutls_ssl_set_client_certicate,
  (int (*)(void *, unsigned char *, size_t)) mailstream_gnutls_ssl_set_client_certificate_data,
  (int (*)(void *, unsigned char *, size_t)) mailstream_gnutls_ssl_set_client_private_key_data,
  (int (*)(void *, char *, char *)) mailstream_gnutls_ssl_set_server_certicate,
  (int (*)(void *, const char *)) mailstream_gnutls_ssl_set_server_name,
  NULL,
  (int (*)(void *)) mailstream_gnutls_ssl_get_fd,
  mailstream_gnutls_ssl_init_not_required,
  mailstream_gnutls_ssl_init_lock,
  mailstream_gnutls_ssl_uninit_lock,
};

const struct mailstream_ssl_backend_driver *
mailstream_gnutls_backend_driver(void)
{
  mailstream_gnutls_driver.low_driver =
    mailstream_gnutls_ssl_driver;
  return &mailstream_gnutls_driver;
}


#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif
#endif /* HAVE_GNUTLS */

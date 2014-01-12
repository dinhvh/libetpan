#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailsasl.h"

#ifdef USE_SASL


#ifdef LIBETPAN_REENTRANT
#if defined(HAVE_PTHREAD_H) && !defined(IGNORE_PTHREAD_H)
#include <pthread.h>
#elif (defined WIN32)
#include <windows.h>
#endif
#endif

#include <sasl/sasl.h>
#include <stdlib.h>

#ifdef LIBETPAN_REENTRANT
#if defined(HAVE_PTHREAD_H) && !defined(IGNORE_PTHREAD_H)
static pthread_mutex_t sasl_lock = PTHREAD_MUTEX_INITIALIZER;
#elif (defined WIN32)
static CRITICAL_SECTION sasl_lock = { 0 };
static int sasl_lock_init_done =  0;
#endif
#endif

static int sasl_use_count = 0;


#ifdef LIBETPAN_REENTRANT
#if defined(HAVE_PTHREAD_H) && !defined(IGNORE_PTHREAD_H)
#elif (defined WIN32)
void mailsasl_init_lock(){
	static int mainsasl_init_lock_done = 0;
	if (InterlockedExchange(&mainsasl_init_lock_done, 1) == 0){
		InitializeCriticalSection(&sasl_lock);
	}
}
#endif
#endif

void mailsasl_external_ref(void)
{
#ifdef LIBETPAN_REENTRANT
#if defined(HAVE_PTHREAD_H) && !defined(IGNORE_PTHREAD_H)
	pthread_mutex_lock(&sasl_lock);
#elif (defined WIN32)
	EnterCriticalSection(&sasl_lock);
#endif
#endif
  sasl_use_count ++;
#ifdef LIBETPAN_REENTRANT
#if defined(HAVE_PTHREAD_H) && !defined(IGNORE_PTHREAD_H)
  pthread_mutex_unlock(&sasl_lock);
#elif (defined WIN32)
  LeaveCriticalSection(&sasl_lock);
#endif
#endif
}

void mailsasl_ref(void)
{
#ifdef LIBETPAN_REENTRANT
#if defined(HAVE_PTHREAD_H) && !defined(IGNORE_PTHREAD_H)
	pthread_mutex_lock(&sasl_lock);
#elif (defined WIN32)
	EnterCriticalSection(&sasl_lock);
#endif
#endif
  sasl_use_count ++;
  if (sasl_use_count == 1)
    sasl_client_init(NULL);
#ifdef LIBETPAN_REENTRANT
#if defined(HAVE_PTHREAD_H) && !defined(IGNORE_PTHREAD_H)
  pthread_mutex_unlock(&sasl_lock);
#elif (defined WIN32)
  LeaveCriticalSection(&sasl_lock);
#endif
#endif
}

void mailsasl_unref(void)
{
#ifdef LIBETPAN_REENTRANT
#if defined(HAVE_PTHREAD_H) && !defined(IGNORE_PTHREAD_H)
	pthread_mutex_lock(&sasl_lock);
#elif (defined WIN32)
	EnterCriticalSection(&sasl_lock);
#endif
#endif
  sasl_use_count --;
  if (sasl_use_count == 0) {
#if 0 /* workaround libsasl bug */
    sasl_done();
#endif
  }
#ifdef LIBETPAN_REENTRANT
#if defined(HAVE_PTHREAD_H) && !defined(IGNORE_PTHREAD_H)
  pthread_mutex_unlock(&sasl_lock);
#elif (defined WIN32)
  LeaveCriticalSection(&sasl_lock);
#endif
#endif
}

#else

void mailsasl_external_ref(void)
{
}

void mailsasl_ref(void)
{
}

void mailsasl_unref(void)
{
}

#endif

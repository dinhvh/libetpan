/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mailhttp.h"

#if defined(HAVE_NSURLSESSION) || defined(__APPLE__)

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

struct mailhttp_nsurlsession_context {
  NSURLSession * session;
};

static int nsurl_error(NSError * error)
{
  if (error == nil)
    return MAILHTTP_NO_ERROR;
  switch ([error code]) {
  case NSURLErrorTimedOut:
    return MAILHTTP_ERROR_TIMEOUT;
  case NSURLErrorBadURL:
  case NSURLErrorUnsupportedURL:
    return MAILHTTP_ERROR_BAD_URL;
  case NSURLErrorCannotFindHost:
  case NSURLErrorDNSLookupFailed:
    return MAILHTTP_ERROR_RESOLVE;
  case NSURLErrorCannotConnectToHost:
  case NSURLErrorNetworkConnectionLost:
  case NSURLErrorNotConnectedToInternet:
    return MAILHTTP_ERROR_CONNECT;
  case NSURLErrorSecureConnectionFailed:
  case NSURLErrorServerCertificateHasBadDate:
  case NSURLErrorServerCertificateUntrusted:
  case NSURLErrorServerCertificateHasUnknownRoot:
  case NSURLErrorServerCertificateNotYetValid:
  case NSURLErrorClientCertificateRejected:
  case NSURLErrorClientCertificateRequired:
    return MAILHTTP_ERROR_TLS;
  case NSURLErrorCancelled:
    return MAILHTTP_ERROR_CANCELLED;
  default:
    return MAILHTTP_ERROR_IO;
  }
}

static int nsurlsession_perform(struct mailhttp_transport * transport,
    struct mailhttp_request * request,
    struct mailhttp_response ** result)
{
  __block struct mailhttp_response * response = NULL;
  __block int result_code = MAILHTTP_ERROR_IO;
  dispatch_semaphore_t semaphore;
  NSMutableURLRequest * ns_request;
  NSURLSessionDataTask * task;
  struct mailhttp_nsurlsession_context * context;
  clistiter * cur;

  if ((request == NULL) || (result == NULL))
    return MAILHTTP_ERROR_BAD_STATE;
  context = transport->data;
  if ((context == NULL) || (context->session == nil))
    return MAILHTTP_ERROR_BAD_STATE;
  * result = NULL;

  @autoreleasepool {
    NSURL * url = [NSURL URLWithString:
        [NSString stringWithUTF8String:request->url]];
    if (url == nil)
      return MAILHTTP_ERROR_BAD_URL;
    ns_request = [NSMutableURLRequest requestWithURL:url];
    if (ns_request == nil)
      return MAILHTTP_ERROR_MEMORY;
    [ns_request setHTTPMethod:[NSString stringWithUTF8String:request->method]];
    [ns_request setTimeoutInterval:(NSTimeInterval) request->timeout];
    for (cur = clist_begin(request->headers); cur != NULL;
        cur = clist_next(cur)) {
      struct mailhttp_header * header = clist_content(cur);
      NSString * name = [NSString stringWithUTF8String:header->name];
      NSString * value = [NSString stringWithUTF8String:header->value];
      if ((name == nil) || (value == nil))
        return MAILHTTP_ERROR_BAD_STATE;
      [ns_request addValue:value forHTTPHeaderField:name];
    }
    if (request->body_len != 0) {
      NSData * body = [NSData dataWithBytes:request->body
          length:request->body_len];
      if (body == nil)
        return MAILHTTP_ERROR_MEMORY;
      [ns_request setHTTPBody:body];
    }
    semaphore = dispatch_semaphore_create(0);
    if (semaphore == NULL)
      return MAILHTTP_ERROR_MEMORY;
    task = [context->session dataTaskWithRequest:ns_request
        completionHandler:^(NSData * data, NSURLResponse * url_response,
            NSError * error) {
      NSHTTPURLResponse * http_response;
      if (error != nil) {
        result_code = nsurl_error(error);
        dispatch_semaphore_signal(semaphore);
        return;
      }
      if (![url_response isKindOfClass:[NSHTTPURLResponse class]]) {
        result_code = MAILHTTP_ERROR_PROTOCOL;
        dispatch_semaphore_signal(semaphore);
        return;
      }
      http_response = (NSHTTPURLResponse *) url_response;
      response = mailhttp_response_new((int) [http_response statusCode]);
      if (response == NULL) {
        result_code = MAILHTTP_ERROR_MEMORY;
        dispatch_semaphore_signal(semaphore);
        return;
      }
      for (id key in [http_response allHeaderFields]) {
        id value = [[http_response allHeaderFields] objectForKey:key];
        if (mailhttp_response_add_header(response,
            [[key description] UTF8String],
            [[value description] UTF8String]) != MAILHTTP_NO_ERROR) {
          result_code = MAILHTTP_ERROR_MEMORY;
          dispatch_semaphore_signal(semaphore);
          return;
        }
      }
      if (mailhttp_response_set_final_url(response,
          [[[http_response URL] absoluteString] UTF8String]) !=
          MAILHTTP_NO_ERROR) {
        result_code = MAILHTTP_ERROR_MEMORY;
        dispatch_semaphore_signal(semaphore);
        return;
      }
      if ((data != nil) && ([data length] != 0)) {
        if (request->body_sink != NULL) {
          if (request->body_sink([data bytes], [data length],
              request->body_sink_context) != 0) {
            result_code = MAILHTTP_ERROR_BODY_SINK;
            dispatch_semaphore_signal(semaphore);
            return;
          }
        }
        else if (mailhttp_response_append_body(response, [data bytes],
            [data length]) != MAILHTTP_NO_ERROR) {
          result_code = MAILHTTP_ERROR_MEMORY;
          dispatch_semaphore_signal(semaphore);
          return;
        }
      }
      result_code = MAILHTTP_NO_ERROR;
      dispatch_semaphore_signal(semaphore);
    }];
    if (task == nil)
      return MAILHTTP_ERROR_IO;
    [task resume];
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  }

  if (result_code != MAILHTTP_NO_ERROR) {
    mailhttp_response_free(response);
    return result_code;
  }
  * result = response;
  return MAILHTTP_NO_ERROR;
}

static void nsurlsession_free(struct mailhttp_transport * transport)
{
  struct mailhttp_nsurlsession_context * context;

  if (transport == NULL)
    return;
  context = transport->data;
  if (context == NULL)
    return;
  [context->session invalidateAndCancel];
#if !__has_feature(objc_arc)
  [context->session release];
#endif
  free(context);
}

static const struct mailhttp_transport_driver nsurlsession_driver = {
  nsurlsession_perform,
  nsurlsession_free
};

int mailhttp_transport_new_nsurlsession(struct mailhttp_transport ** result)
{
  struct mailhttp_nsurlsession_context * context;
  NSURLSessionConfiguration * configuration;

  if (result == NULL)
    return MAILHTTP_ERROR_BAD_STATE;
  * result = NULL;
  context = calloc(1, sizeof(* context));
  if (context == NULL)
    return MAILHTTP_ERROR_MEMORY;
  configuration = [NSURLSessionConfiguration defaultSessionConfiguration];
  context->session = [NSURLSession sessionWithConfiguration:configuration];
  if (context->session == nil) {
    free(context);
    return MAILHTTP_ERROR_MEMORY;
  }
#if !__has_feature(objc_arc)
  [context->session retain];
#endif
  * result = mailhttp_transport_new(context, &nsurlsession_driver,
      MAILHTTP_BACKEND_NSURLSESSION);
  if (* result == NULL) {
    [context->session invalidateAndCancel];
#if !__has_feature(objc_arc)
    [context->session release];
#endif
    free(context);
    return MAILHTTP_ERROR_MEMORY;
  }
  return MAILHTTP_NO_ERROR;
}

#else

int mailhttp_transport_new_nsurlsession(struct mailhttp_transport ** result)
{
  if (result == NULL)
    return MAILHTTP_ERROR_BAD_STATE;
  * result = NULL;
  return MAILHTTP_ERROR_UNAVAILABLE;
}

#endif

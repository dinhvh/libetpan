/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailgmail_http.h"

#if defined(__APPLE__)

#import <dispatch/dispatch.h>
#import <Foundation/Foundation.h>

#include <stdlib.h>

static int nsurlsession_perform(struct mailgmail_http_transport * transport,
    struct mailgmail_http_request * request,
    struct mailgmail_http_response ** result)
{
  __block struct mailgmail_http_response * response;
  __block int result_code;
  dispatch_semaphore_t semaphore;
  NSMutableURLRequest * ns_request;
  NSURLSessionDataTask * task;
  clistiter * cur;

  (void) transport;

  if ((request == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  response = NULL;
  result_code = MAILGMAIL_ERROR_HTTP;

  @autoreleasepool {
    NSURL * url;
    NSData * body;

    url = [NSURL URLWithString:[NSString stringWithUTF8String:request->url]];
    if (url == nil)
      return MAILGMAIL_ERROR_BAD_STATE;

    ns_request = [NSMutableURLRequest requestWithURL:url];
    if (ns_request == nil)
      return MAILGMAIL_ERROR_MEMORY;

    [ns_request setHTTPMethod:
        [NSString stringWithUTF8String:request->method]];
    [ns_request setTimeoutInterval:(NSTimeInterval) request->timeout];

    for (cur = request->headers != NULL ? clist_begin(request->headers) : NULL;
        cur != NULL; cur = clist_next(cur)) {
      struct mailgmail_http_header * header;
      NSString * name;
      NSString * value;

      header = clist_content(cur);
      name = [NSString stringWithUTF8String:header->name];
      value = [NSString stringWithUTF8String:header->value];
      if ((name == nil) || (value == nil))
        return MAILGMAIL_ERROR_BAD_STATE;
      [ns_request setValue:value forHTTPHeaderField:name];
    }

    if (request->body_len > 0) {
      body = [NSData dataWithBytes:request->body length:request->body_len];
      if (body == nil)
        return MAILGMAIL_ERROR_MEMORY;
      [ns_request setHTTPBody:body];
    }

    semaphore = dispatch_semaphore_create(0);
    if (semaphore == NULL)
      return MAILGMAIL_ERROR_MEMORY;

    task = [[NSURLSession sharedSession]
        dataTaskWithRequest:ns_request
        completionHandler:^(NSData * data, NSURLResponse * url_response,
            NSError * error) {
          NSHTTPURLResponse * http_response;

          if (error != nil) {
            result_code = MAILGMAIL_ERROR_HTTP;
            dispatch_semaphore_signal(semaphore);
            return;
          }

          if (![url_response isKindOfClass:[NSHTTPURLResponse class]]) {
            result_code = MAILGMAIL_ERROR_PROTOCOL;
            dispatch_semaphore_signal(semaphore);
            return;
          }

          http_response = (NSHTTPURLResponse *) url_response;
          response = mailgmail_http_response_new((int)
              [http_response statusCode]);
          if (response == NULL) {
            result_code = MAILGMAIL_ERROR_MEMORY;
            dispatch_semaphore_signal(semaphore);
            return;
          }

          for (NSString * key in [http_response allHeaderFields]) {
            id value;

            value = [[http_response allHeaderFields] objectForKey:key];
            if ((key != nil) && (value != nil)) {
              if (mailgmail_http_response_add_header(response,
                  [key UTF8String],
                  [[value description] UTF8String]) != MAILGMAIL_NO_ERROR) {
                result_code = MAILGMAIL_ERROR_MEMORY;
                dispatch_semaphore_signal(semaphore);
                return;
              }
            }
          }

          if ((data != nil) && ([data length] > 0)) {
            response->body = malloc([data length]);
            if (response->body == NULL) {
              result_code = MAILGMAIL_ERROR_MEMORY;
              dispatch_semaphore_signal(semaphore);
              return;
            }
            memcpy(response->body, [data bytes], [data length]);
            response->body_len = [data length];
          }

          result_code = MAILGMAIL_NO_ERROR;
          dispatch_semaphore_signal(semaphore);
        }];

    if (task == nil)
      return MAILGMAIL_ERROR_HTTP;

    [task resume];
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  }

  if (result_code != MAILGMAIL_NO_ERROR) {
    mailgmail_http_response_free(response);
    return result_code;
  }

  * result = response;
  return MAILGMAIL_NO_ERROR;
}

int mailgmail_http_transport_new_nsurlsession(
    struct mailgmail_http_transport ** result)
{
  struct mailgmail_http_transport * transport;

  if (result == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;

  transport = malloc(sizeof(* transport));
  if (transport == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  transport->context = NULL;
  transport->perform = nsurlsession_perform;
  transport->free = NULL;

  * result = transport;
  return MAILGMAIL_NO_ERROR;
}

#else

int mailgmail_http_transport_new_nsurlsession(
    struct mailgmail_http_transport ** result)
{
  if (result == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  return MAILGMAIL_ERROR_HTTP_UNAVAILABLE;
}

#endif

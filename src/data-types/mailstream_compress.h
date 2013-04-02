//
//  mailstream_compress.h
//  libetpan
//
//  Created by Ian Ragsdale on 3/8/13.
//
//

#ifndef MAILSTREAM_COMPRESS_H
#define MAILSTREAM_COMPRESS_H

#define USE_DEFLATE 1

#include <libetpan/mailstream.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    /* socket */
    
extern mailstream_low_driver * mailstream_compress_driver;

struct mailstream_compress_context;

// exported methods
LIBETPAN_EXPORT
mailstream_low * mailstream_low_compress_open(mailstream_low *ms);

LIBETPAN_EXPORT
ssize_t mailstream_low_compress_read(mailstream_low * s, void * buf, size_t count);

LIBETPAN_EXPORT
ssize_t mailstream_low_compress_write(mailstream_low * s, const void * buf, size_t count);

LIBETPAN_EXPORT
int mailstream_low_compress_close(mailstream_low * s);

    
LIBETPAN_EXPORT
int mailstream_low_compress_get_fd(mailstream_low * s);

    
LIBETPAN_EXPORT
struct mailstream_cancel * mailstream_low_compress_get_cancel(mailstream_low * s);

LIBETPAN_EXPORT
void mailstream_low_compress_free(mailstream_low * s);

LIBETPAN_EXPORT
void mailstream_low_compress_cancel(mailstream_low * s);

#endif
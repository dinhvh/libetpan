#ifndef __SYSCALL_WRAPPERS_H__
#define __SYSCALL_WRAPPERS_H__

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include "connect.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline FILE * Fopen(const char *filename, const char *mode)
{
    FILE * f;

    do {
        f = fopen(filename, mode);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (f == NULL && errno == EINTR);

    return f;
}
#define fopen >@<

static inline FILE * Fdopen(int fildes, const char *mode)
{
    FILE * f;

    do {
        f = fdopen(fildes, mode);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (f == NULL && errno == EINTR);

    return f;
}
#define fdopen >@<

static inline char *Fgets(char * str, int size, FILE * stream)
{
    char * s;

    do {
        s = fgets(str, size, stream);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (s == NULL && errno == EINTR);

    return s;
}
#define fgets >@<

static inline int Fputs(const char *str, FILE * stream)
{
    int r;

    do {
        r = fputs(str, stream);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == EOF && errno == EINTR);

    return r;
}
#define fputs >@<

static inline int Fclose(FILE *stream)
{
    int r;

    do {
        r = fclose(stream);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == EOF && errno == EINTR);

    return r;
}
#define fclose >@<

static inline FILE * Freopen(
        const char *filename,
        const char *mode,
        FILE * stream
    )
{
    FILE * f;

    do {
        f = freopen(filename, mode, stream);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (f == NULL && errno == EINTR);

    return f;
}
#define freopen >@<

static inline int Fprintf(FILE * stream, const char * format, ...)
{
    int n;
    va_list arglist;

    va_start(arglist, format);

    do {
        n = vfprintf(stream, format, arglist);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (n < 0 && errno == EINTR);

    va_end( arglist );

    return n;
}
#define fprintf >@<

static inline size_t Fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream)
{
    size_t r = 0;

    do {
        clearerr(stream);
        size_t n = fwrite((char *) ptr + r, size, nitems, stream);
        nitems -= n;
        r += n * size;

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (nitems && ferror(stream) == EINTR);

    return r;
}
#define fwrite >@<

static inline size_t Fread(void *ptr, size_t size, size_t nitems, FILE *stream)
{
    size_t r = 0;

    do {
        clearerr(stream);
        size_t n = fread((char *) ptr + r, size, nitems, stream);
        nitems -= n;
        r += n * size;

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (!feof(stream) && nitems && ferror(stream) == EINTR);

    return r;
}
#define fread >@<

static inline int Fflush(FILE *stream)
{
    int r;

    do {
        r = fflush(stream);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == -1 && errno == EINTR);

    return r;
}
#define fflush >@<

static inline int Mkstemp(char *template)
{
    int fd;

    do {
        fd = mkstemp(template);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (fd == -1 && errno == EINTR);

    return fd;
}
#define mkstemp >@<

static inline int Close(int fildes)
{
    int r;

    do {
        r = close(fildes);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == -1 && errno == EINTR);

    return r;
}
#define close >@<

static inline ssize_t Write(int fildes, const void *buf, size_t nbyte)
{
    ssize_t r;

    do {
        r = write(fildes, buf, nbyte);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == -1 && errno == EINTR);

    return r;
}
#define write >@<

static inline ssize_t Read(int fildes, void *buf, size_t nbyte)
{
    ssize_t r;

    do {
        r = read(fildes, buf, nbyte);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == -1 && errno == EINTR);

    return r;
}
#define read >@<

static inline int Ftruncate(int fildes, off_t length)
{
    int r;

    do {
        r = ftruncate(fildes, length);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == -1 && errno == EINTR);

    return r;
}
#define ftruncate >@<

static inline int Dup2(int fildes, int fildes2)
{
    int fd;

    do {
        fd = dup2(fildes, fildes2);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (fd == -1 && errno == EINTR);

    return fd;
}
#define dup2 >@<

static inline int Select(
        int nfds,
        fd_set *readfds,
        fd_set *writefds,
        fd_set *errorfds,
        struct timeval *timeout
    )
{
    int r;

    do {
        r = select(nfds, readfds, writefds, errorfds, timeout);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == -1 && errno == EINTR);

    return r;
}
#define select >@<

static inline int Fcntl(int fildes, int cmd, void *structure)
{
    int r;

    assert(cmd == F_SETLKW);

    do {
        r = fcntl(fildes, cmd, structure);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == -1 && errno == EINTR);

    return r;
}
// fnctl does only risk EINTR if cmd == F_SETLKW

static inline int Open(const char *path, int oflag, ...)
{
    int fd;

    do {
        fd = open(path, oflag, 0);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (fd == -1 && errno == EINTR);

    return fd;
}
#define open >@<

static inline int Creat(const char *path, mode_t mode)
{
    int fd;

    do {
        fd = creat(path, mode);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (fd == -1 && errno == EINTR);

    return fd;
}
#define creat >@<

static inline ssize_t Recv(int socket, void *buffer, size_t length, int flags)
{
    ssize_t r;

    do {
        r = recv(socket, buffer, length, flags);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == -1 && errno == EINTR);

    return r;
}
#define recv >@<

static inline ssize_t Send(int socket, const void *buffer, size_t length, int flags)
{
    ssize_t r;

    do {
        r = send(socket, buffer, length, flags);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == -1 && errno == EINTR);

    return r;
}
#define send >@<

static inline pid_t Waitpid(pid_t pid, int *stat_loc, int options)
{
    pid_t r;

    do {
        r = waitpid(pid, stat_loc, options);

        if (libetpan_cancel_read_write) {
            libetpan_cancel_read_write = 0;
            break;
        }
    } while (r == -1 && errno == EINTR);

    return r;
}
#define waitpid >@<

#ifdef __cplusplus
}
#endif

#endif // __SYSCALL_WRAPPERS_H__


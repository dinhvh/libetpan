#pragma once

#include <errno.h>
#include <stdarg.h>

#ifdef EOF // stdio.h

static inline FILE * Fopen(const char *filename, const char *mode)
{
    FILE * f;

    do {
        f = fopen(filename, mode);
    } while (f == NULL && errno == EINTR);

    return f;
}
#define fopen >@<

static inline FILE * Fdopen(int fildes, const char *mode)
{
    FILE * f;

    do {
        f = fdopen(fildes, mode);
    } while (f == NULL && errno == EINTR);

    return f;
}
#define fdopen >@<

static inline char *Fgets(char * str, int size, FILE * stream)
{
    char * s;

    do {
        s = fgets(str, size, stream);
    } while (s == NULL && errno == EINTR);

    return s;
}
#define fgets >@<

static inline int Fputs(const char *str, FILE * stream)
{
    int r;

    do {
        r = fputs(str, stream);
    } while (r == EOF && errno == EINTR);

    return r;
}
#define fputs >@<

static inline int Fclose(FILE *stream)
{
    int r;

    do {
        r = fclose(stream);
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
    } while (!feof(stream) && nitems && ferror(stream) == EINTR);

    return r;
}
#define fread >@<

static inline int Fflush(FILE *stream)
{
    int r;

    do {
        r = fflush(stream);
    } while (r == -1 && errno == EINTR);

    return r;
}
#define fflush >@<

#endif // stdio.h

#ifdef F_OK // unistd.h

static inline int Mkstemp(char *template)
{
    int fd;

    do {
        fd = mkstemp(template);
    } while (fd == -1 && errno == EINTR);

    return fd;
}
#define mkstemp >@<

static inline int Close(int fildes)
{
    int r;

    do {
        r = close(fildes);
    } while (r == -1 && errno == EINTR);

    return r;
}
#define close >@<

static inline ssize_t Write(int fildes, const void *buf, size_t nbyte)
{
    ssize_t r;

    do {
        r = write(fildes, buf, nbyte);
    } while (r == -1 && errno == EINTR);

    return r;
}
#define write >@<

static inline ssize_t Read(int fildes, void *buf, size_t nbyte)
{
    ssize_t r;

    do {
        r = read(fildes, buf, nbyte);
    } while (r == -1 && errno == EINTR);

    return r;
}
#define read >@<

static inline int Ftruncate(int fildes, off_t length)
{
    int r;

    do {
        r = ftruncate(fildes, length);
    } while (r == -1 && errno == EINTR);

    return r;
}
#define ftruncate >@<

static inline int Dup2(int fildes, int fildes2)
{
    int fd;

    do {
        fd = dup2(fildes, fildes2);
    } while (fd == -1 && errno == EINTR);

    return fd;
}
#define dup2 >@<

#endif

#ifdef FD_CLR // select.h

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
    } while (r == -1 && errno == EINTR);

    return r;
}
#define select >@<

#endif // select.h

#ifdef F_SETLKW // fcntl.h

static inline int Fcntl(int fildes, int cmd, void *structure)
{
    int r;

    do {
        r = fcntl(fildes, cmd, structure);
    } while (r == -1 && errno == EINTR);

    return r;
}
// fnctl does only risk EINTR if cmd == F_SETLKW

static inline int Open(const char *path, int oflag, ...)
{
    int fd;

    do {
        fd = open(path, oflag, 0);
    } while (fd == -1 && errno == EINTR);

    return fd;
}
#define open >@<

static inline int Creat(const char *path, mode_t mode)
{
    int fd;

    do {
        fd = creat(path, mode);
    } while (fd == -1 && errno == EINTR);

    return fd;
}
#define creat >@<

#endif // fcntl.h

#ifdef MSG_PEEK // socket.h

static inline ssize_t Recv(int socket, void *buffer, size_t length, int flags)
{
    ssize_t r;

    do {
        r = recv(socket, buffer, length, flags);
    } while (r == -1 && errno == EINTR);

    return r;
}
#define recv >@<

static inline ssize_t Send(int socket, const void *buffer, size_t length, int flags)
{
    ssize_t r;

    do {
        r = send(socket, buffer, length, flags);
    } while (r == -1 && errno == EINTR);

    return r;
}
#define send >@<

#endif // socket.h

#ifdef WNOHANG // wait.h

static inline pid_t Waitpid(pid_t pid, int *stat_loc, int options)
{
    pid_t r;

    do {
        r = waitpid(pid, stat_loc, options);
    } while (r == -1 && errno == EINTR);

    return r;
}
#define waitpid >@<

#endif


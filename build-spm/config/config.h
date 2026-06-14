/*
 * Static config for Swift Package Manager builds.
 *
 * SPM does not run configure, so this file captures the intended Apple source
 * package configuration. Keep it aligned with Package.swift.
 */

#ifndef LIBETPAN_SPM_CONFIG_H
#define LIBETPAN_SPM_CONFIG_H

#define DBVERS 1

#define HAVE_ARPA_INET_H 1
#define HAVE_CFNETWORK 1
#define HAVE_CTYPE_H 1
#define HAVE_DLFCN_H 1
#define HAVE_FCNTL_H 1
#define HAVE_GETOPT_LONG 1
#define HAVE_GETPAGESIZE 1
#define HAVE_ICONV 1
#define HAVE_INTTYPES_H 1
#define HAVE_IPV6 1
#define HAVE_LIMITS_H 1
#define HAVE_MMAP 1
#define HAVE_NETDB_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_PTHREAD_H 1
#define HAVE_SETENV 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_MMAN_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_ZLIB 1

#define LIBETPAN_IOS_DISABLE_SSL 1
#define LIBETPAN_REENTRANT 1
#define LIBETPAN_VERSION "1.10.1"
#define LIBETPAN_VERSION_MAJOR 1
#define LIBETPAN_VERSION_MINOR 10
#define LIBETPAN_VERSION_MICRO 1

#define PACKAGE "libetpan"
#define PACKAGE_BUGREPORT "libetpan-devel@lists.sourceforge.net"
#define PACKAGE_NAME "libetpan"
#define PACKAGE_STRING "libetpan 1.10.1"
#define PACKAGE_TARNAME "libetpan"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "1.10.1"
#define STDC_HEADERS 1
#define UNSTRICT_SYNTAX 1
#define VERSION "1.10.1"

/* SASL is disabled for the source package to avoid a system/vendor dependency. */
/* #undef USE_SASL */

/* OpenSSL, GnuTLS, curl, expat, lmdb, and poll are disabled for SPM. */
/* #undef HAVE_CURL */
/* #undef HAVE_EXPAT */
/* #undef HAVE_ICONV_PROTO_CONST */
/* #undef HAVE_LIBLOCKFILE */
/* #undef HAVE_MINGW32_SYSTEM */
/* #undef HAVE_SYS_POLL_H */
/* #undef HAVE_WINSOCK2_H */
/* #undef LMDB */
/* #undef USE_GNUTLS */
/* #undef USE_POLL */
/* #undef USE_SSL */

#endif

/*
 * Simple Mail Submission Agent using SMTP with libEtPan!
 * TODO: Full sendmail like interface
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/libetpan.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef WIN32
#	include "win_etpan.h"
#ifdef _MSC_VER
#	include "../src/bsd/getopt.h"
#	define STDIN_FILENO _fileno(stdin)
#else
#	include <getopt.h>
#endif
#else
#	include <netdb.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#	include <sys/mman.h>
#	include <unistd.h>
#	include <sys/ioctl.h>
#	include <pwd.h>

#	define _GNU_SOURCE
#	include <getopt.h>
#endif

/* globals */
char *smtp_server;
uint16_t smtp_port = 25;
char *smtp_user;
char *smtp_password;
char *smtp_from;
int smtp_tls = 0;
int smtp_esmtp = 1;
int smtp_ssl = 0;
int smtp_lmtp = 0;

struct mem_message {
  char *data;
  size_t len;
  MMAPString *mstring;
};

#define BLOCKSIZE 4096

int collect(struct mem_message *message) {
  int len;

  memset(message, 0, sizeof(struct mem_message));

#ifndef MMAP_UNAVAILABLE
  struct stat sb;
  /* if stdin is a file whose size is known, try to mmap it */
  if (!fstat(0, &sb) && S_ISREG(sb.st_mode) && sb.st_size >= 0) {
    message->len = sb.st_size;
    if ((message->data = mmap(NULL, message->len, PROT_READ, MAP_SHARED,
			      STDIN_FILENO, 0)) != MAP_FAILED)
      return 0;
  }
#endif

  /* read the buffer from stdin by blocks, until EOF or error.
     save the message in a mmap_string */
  if ((message->mstring = mmap_string_sized_new(BLOCKSIZE)) == NULL) {
    perror("mmap_string_new");
    goto error;
  }
  message->len = 0;

  while ((len = read(STDIN_FILENO,
		     message->mstring->str + message->len, BLOCKSIZE)) > 0) {
    message->len += len;
    /* reserve room for next block */
    if ((mmap_string_set_size(message->mstring,
			      message->len + BLOCKSIZE)) == NULL) {
      perror("mmap_string_set_size");
      goto error;
    }
  }

  if (len == 0) {
    message->data = message->mstring->str;
    return 0; /* OK */
  }

  perror("read");

 error:
  if (message->mstring != NULL)
    mmap_string_free(message->mstring);
  return -1;
}

char *guessfrom(void) {
#ifndef WIN32
  uid_t uid;
  struct passwd *pw;
  char hostname[256];
  int len;
  char *gfrom;

  if (gethostname(hostname, sizeof(hostname))) {
    perror("gethostname");
    return NULL;
  }
  hostname[sizeof(hostname) - 1] = '\0';

  uid = getuid();
  pw = getpwuid(uid);
  
  len = ((pw != NULL) ? strlen(pw->pw_name) : 12)
    + strlen(hostname) + 2;

  if ((gfrom = malloc(len)) == NULL) {
    perror("malloc");
    return NULL;
  }
  if (pw != NULL && pw->pw_name != NULL) 
    snprintf(gfrom, len, "%s@%s", pw->pw_name, hostname);
  else
    snprintf(gfrom, len, "#%u@%s", uid, hostname);
  return gfrom;
#else
	return NULL;
#endif
}

void release(struct mem_message *message) {
  if (message->mstring != NULL)
    mmap_string_free(message->mstring);
#ifndef MMAP_UNAVAILABLE
  else if (message->data != NULL)
    munmap(message->data, message->len);
#endif
}

int send_message(char *data, size_t len, char**rcpts) {
  int s = -1;
  int ret, i;
  char **r;
  int esmtp = 0;
  mailsmtp *smtp = NULL;
  int *retcodes = NULL;
  clist *recipients = clist_new();

  if ((smtp = mailsmtp_new(0, NULL)) == NULL) {
    perror("mailsmtp_new");
    goto error;
  }

  /* first open the stream */
  if(smtp_ssl) {
    // use SMTP over SSL
    if ((ret = mailsmtp_ssl_connect(smtp, 
                                    (smtp_server != NULL ? smtp_server : "localhost"),
                                    smtp_port)) != MAILSMTP_NO_ERROR) {
      fprintf(stderr, "mailsmtp_ssl_connect: %s\n", mailsmtp_strerror(ret));
      goto error;
    }
  } else {
    // use STARTTLS
    if ((ret = mailsmtp_socket_connect(smtp, 
                                       (smtp_server != NULL ? smtp_server : "localhost"),
                                       smtp_port)) != MAILSMTP_NO_ERROR) {
      fprintf(stderr, "mailsmtp_socket_connect: %s\n", mailsmtp_strerror(ret));
      goto error;
    }
  }
  
  /* then introduce ourselves */
  if (smtp_lmtp)
    ret = mailesmtp_lhlo(smtp, "lmtp-test");
  else if (smtp_esmtp && (ret = mailesmtp_ehlo(smtp)) == MAILSMTP_NO_ERROR)
    esmtp = 1;
  else if (!smtp_esmtp || ret == MAILSMTP_ERROR_NOT_IMPLEMENTED)
    ret = mailsmtp_helo(smtp);
  if (ret != MAILSMTP_NO_ERROR) {
    fprintf(stderr, "mailsmtp_helo: %s\n", mailsmtp_strerror(ret));
    goto error;
  }

  if (esmtp && smtp_tls &&
      (ret = mailsmtp_socket_starttls(smtp)) != MAILSMTP_NO_ERROR) {
    fprintf(stderr, "mailsmtp_starttls: %s\n", mailsmtp_strerror(ret));
    goto error;
  }
  
  if (esmtp && smtp_tls) {
    /* introduce ourselves again */
    if (smtp_lmtp)
      ret = mailesmtp_lhlo(smtp, "lmtp-test");
    else if (smtp_esmtp && (ret = mailesmtp_ehlo(smtp)) == MAILSMTP_NO_ERROR)
      esmtp = 1;
    else if (!smtp_esmtp || ret == MAILSMTP_ERROR_NOT_IMPLEMENTED)
      ret = mailsmtp_helo(smtp);
    if (ret != MAILSMTP_NO_ERROR) {
      fprintf(stderr, "mailsmtp_helo: %s\n", mailsmtp_strerror(ret));
      goto error;
    }
  }
  
  if (esmtp && smtp_user != NULL &&
      (ret = mailsmtp_auth(smtp, smtp_user,
			   (smtp_password != NULL) ? smtp_password : ""))
      != MAILSMTP_NO_ERROR) {
    fprintf(stderr, "mailsmtp_auth: %s: %s\n", smtp_user, mailsmtp_strerror(ret));
    goto error;
  }

  /* source */
  if ((ret = (esmtp ?
	      mailesmtp_mail(smtp, smtp_from, 1, "etPanSMTPTest") :
	      mailsmtp_mail(smtp, smtp_from))) != MAILSMTP_NO_ERROR) {
    fprintf(stderr, "mailsmtp_mail: %s, %s\n", smtp_from, mailsmtp_strerror(ret));
    goto error;
  }
  
  /* recipients */
  for (r = rcpts; *r != NULL; r++) {
    if ((ret = (esmtp ?
		mailesmtp_rcpt(smtp, *r,
			       MAILSMTP_DSN_NOTIFY_FAILURE|MAILSMTP_DSN_NOTIFY_DELAY,
			       NULL) :
		mailsmtp_rcpt(smtp, *r))) != MAILSMTP_NO_ERROR) {
      fprintf(stderr, "mailsmtp_rcpt: %s: %s\n", *r, mailsmtp_strerror(ret));
      goto error;
    }else{
        clist_append(recipients, *r);
    }
  }

  /* message */
  if ((ret = mailsmtp_data(smtp)) != MAILSMTP_NO_ERROR) {
    fprintf(stderr, "mailsmtp_data: %s\n", mailsmtp_strerror(ret));
    goto error;
  }
  if (smtp_lmtp){
      retcodes = malloc((clist_count(recipients) * sizeof(int)));
    if ((ret = maillmtp_data_message(smtp, data, len, recipients, retcodes)) != MAILSMTP_NO_ERROR) {
        fprintf(stderr, "maillmtp_data: %s (%d)\n", mailsmtp_strerror(ret), ret);
        goto error;
    }
    for (i = 0; i < clist_count(recipients); i++)
      fprintf(stderr, "recipient %s returned: %d\n", (char *)clist_nth_data(recipients, i), retcodes[i]);
  } else if ((ret = mailsmtp_data_message(smtp, data, len)) != MAILSMTP_NO_ERROR) {
        fprintf(stderr, "mailsmtp_data_message: %s\n", mailsmtp_strerror(ret));
        goto error;
  }
  mailsmtp_free(smtp);
  return 0;

 error:
  if (smtp != NULL)
    mailsmtp_free(smtp);
  free(retcodes);
  clist_free(recipients);
  if (s >= 0)
    close(s);
  return -1;
}

int main(int argc, char **argv) {
  struct mem_message message;
  int r;


#if HAVE_GETOPT_LONG
  int indx;
  static struct option long_options[] = {
    {"server",   1, 0, 's'},
    {"port",     1, 0, 'p'},
    {"user",     1, 0, 'u'},
    {"password", 1, 0, 'v'},
    {"from",     1, 0, 'f'},
    {"tls",      0, 0, 'S'},
    {"no-esmtp", 0, 0, 'E'},
    {"ssl",      0, 0, 'L'},
    {"lmtp",     0, 0, 'T'},
    {NULL,       0, 0, 0},
  };
#endif

  while(1) {
#if HAVE_GETOPT_LONG
	r = getopt_long(argc, argv, "s:p:u:v:f:SELT", long_options, &indx);
#else
	r = getopt(argc, argv, "s:p:u:v:f:SELT");
#endif
    if (r < 0)
      break;
    switch (r) {
    case 's':
      if (smtp_server != NULL)
	free(smtp_server);
      smtp_server = strdup(optarg);
      break;
    case 'p':
      smtp_port = (uint16_t) strtoul(optarg, NULL, 10);
      break;
    case 'u':
      if (smtp_user != NULL)
	free(smtp_user);
      smtp_user = strdup(optarg);
      break;
    case 'v':
      if (smtp_password != NULL)
	free(smtp_password);
      smtp_password = strdup(optarg);
      break;
    case 'f':
      if (smtp_from != NULL)
	free(smtp_from);
      smtp_from = strdup(optarg);
      break;
    case 'S':
      smtp_tls = 1;
      break;
    case 'E':
      smtp_esmtp = 0;
      break;
    case 'L':
      smtp_ssl = 1;
      break;
    case 'T':
      smtp_lmtp = 1;
      break;
    }
  }

  argc -= optind;
  argv += optind;

  if (argc < 1) {
    fprintf(stderr, "usage: smtpsend [-f from] [-u user] [-v password] [-s server] [-p port] [-S] [-L] [-T] <rcpts>...\n");
    return EXIT_FAILURE;
  }

  if (smtp_from == NULL && (smtp_from = guessfrom()) == NULL) {
    fprintf(stderr, "can't guess a valid from, please use -f option.\n");
    return EXIT_FAILURE;
  }
  
  /* reads message from stdin */
  if (collect(&message))
    return EXIT_FAILURE;
  
  send_message(message.data, message.len, argv);

  release(&message);

  fprintf(stdout, "Sent ok.\n");

  return EXIT_SUCCESS;
}

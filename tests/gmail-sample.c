/*
 * Low-level Gmail HTTP smoke sample.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailgmail.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gmail_sample_args {
  const char * user;
  const char * oauth_token;
  unsigned int max_results;
  int fetch_first_message;
};

static void usage(const char * progname)
{
  fprintf(stderr,
      "usage: %s --oauth-token TOKEN [--user USER] [--max-results N] "
      "[--fetch-first-message]\n",
      progname);
}

static int parse_uint(const char * value, unsigned int * result)
{
  char * end;
  unsigned long parsed;

  if ((value == NULL) || (result == NULL))
    return 0;

  parsed = strtoul(value, &end, 10);
  if ((* value == '\0') || (* end != '\0') || (parsed > 500))
    return 0;

  * result = (unsigned int) parsed;
  return 1;
}

static int parse_args(int argc, char ** argv, struct gmail_sample_args * args)
{
  int i;

  args->user = getenv("GMAIL_USER");
  args->oauth_token = getenv("GMAIL_ACCESS_TOKEN");
  args->max_results = 5;
  args->fetch_first_message = 0;

  for (i = 1; i < argc; i ++) {
    if ((strcmp(argv[i], "--oauth-token") == 0) && (i + 1 < argc)) {
      args->oauth_token = argv[++ i];
    }
    else if ((strcmp(argv[i], "--user") == 0) && (i + 1 < argc)) {
      args->user = argv[++ i];
    }
    else if ((strcmp(argv[i], "--max-results") == 0) && (i + 1 < argc)) {
      if (!parse_uint(argv[++ i], &args->max_results))
        return 0;
    }
    else if (strcmp(argv[i], "--fetch-first-message") == 0) {
      args->fetch_first_message = 1;
    }
    else if (strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      exit(0);
    }
    else {
      return 0;
    }
  }

  return (args->oauth_token != NULL) && (* args->oauth_token != '\0');
}

static unsigned int list_count(clist * list)
{
  clistiter * cur;
  unsigned int count;

  count = 0;
  for (cur = list != NULL ? clist_begin(list) : NULL;
      cur != NULL; cur = clist_next(cur))
    count ++;

  return count;
}

static void print_error(mailgmail * session, const char * step, int r)
{
  const char * message;

  fprintf(stderr, "%s failed: %d", step, r);
  if (session != NULL) {
    fprintf(stderr, " http=%d", mailgmail_get_last_http_status(session));
    message = mailgmail_get_last_error_message(session);
    if (message != NULL)
      fprintf(stderr, " message=%s", message);
  }
  fprintf(stderr, "\n");
}

static int fetch_first_message(mailgmail * session,
    struct mailgmail_message_list * messages)
{
  struct mailgmail_message_summary * summary;
  struct mailgmail_message_get_request * request;
  struct mailgmail_message * message;
  clistiter * first;
  int r;

  first = messages->messages != NULL ? clist_begin(messages->messages) : NULL;
  if (first == NULL) {
    printf("Gmail first_message=none\n");
    return 0;
  }

  summary = clist_content(first);
  if ((summary == NULL) || (summary->id == NULL)) {
    fprintf(stderr, "Gmail message list returned an empty summary\n");
    return 1;
  }

  request = mailgmail_message_get_request_new(
      MAILGMAIL_MESSAGE_FORMAT_METADATA);
  if (request == NULL) {
    fprintf(stderr, "Gmail message get request allocation failed\n");
    return 1;
  }

  r = mailgmail_message_get_request_add_metadata_header(request, "Subject");
  if (r != MAILGMAIL_NO_ERROR) {
    mailgmail_message_get_request_free(request);
    print_error(session, "metadata header", r);
    return 1;
  }

  message = NULL;
  r = mailgmail_get_message(session, summary->id, request, &message);
  mailgmail_message_get_request_free(request);
  if (r != MAILGMAIL_NO_ERROR) {
    print_error(session, "messages.get", r);
    return 1;
  }

  printf("Gmail first_message id=%s thread=%s labels=%u size=%u\n",
      message->id != NULL ? message->id : "",
      message->thread_id != NULL ? message->thread_id : "",
      list_count(message->label_ids),
      message->size_estimate);

  mailgmail_message_free(message);
  return 0;
}

int main(int argc, char ** argv)
{
  struct gmail_sample_args args;
  mailgmail * session;
  struct mailgmail_profile * profile;
  struct mailgmail_label_list * labels;
  struct mailgmail_message_list_request * message_request;
  struct mailgmail_message_list * messages;
  int r;
  int status;

  if (!parse_args(argc, argv, &args)) {
    usage(argv[0]);
    return 2;
  }

  session = mailgmail_new();
  if (session == NULL) {
    fprintf(stderr, "Gmail session allocation failed\n");
    return 1;
  }

  status = 1;
  r = mailgmail_set_oauth2_token(session, args.oauth_token);
  if (r != MAILGMAIL_NO_ERROR) {
    print_error(session, "set oauth token", r);
    goto cleanup;
  }

  if (args.user != NULL) {
    r = mailgmail_set_user(session, args.user);
    if (r != MAILGMAIL_NO_ERROR) {
      print_error(session, "set user", r);
      goto cleanup;
    }
  }

  r = mailgmail_set_user_agent(session, "libEtPan Gmail HTTP smoke test");
  if (r != MAILGMAIL_NO_ERROR) {
    print_error(session, "set user agent", r);
    goto cleanup;
  }

  profile = NULL;
  r = mailgmail_get_profile(session, &profile);
  if (r != MAILGMAIL_NO_ERROR) {
    print_error(session, "users.getProfile", r);
    goto cleanup;
  }

  printf("Gmail profile email=%s messages=%u threads=%u history=%s\n",
      profile->email_address != NULL ? profile->email_address : "",
      profile->messages_total,
      profile->threads_total,
      profile->history_id != NULL ? profile->history_id : "");
  mailgmail_profile_free(profile);

  labels = NULL;
  r = mailgmail_list_labels(session, &labels);
  if (r != MAILGMAIL_NO_ERROR) {
    print_error(session, "labels.list", r);
    goto cleanup;
  }
  printf("Gmail labels=%u\n", list_count(labels->labels));
  mailgmail_label_list_free(labels);

  message_request = mailgmail_message_list_request_new();
  if (message_request == NULL) {
    fprintf(stderr, "Gmail message list request allocation failed\n");
    goto cleanup;
  }
  message_request->max_results = args.max_results;

  messages = NULL;
  r = mailgmail_list_messages(session, message_request, &messages);
  mailgmail_message_list_request_free(message_request);
  if (r != MAILGMAIL_NO_ERROR) {
    print_error(session, "messages.list", r);
    goto cleanup;
  }

  printf("Gmail messages_page_count=%u result_size_estimate=%u next_page=%s\n",
      list_count(messages->messages),
      messages->result_size_estimate,
      messages->next_page_token != NULL ? "yes" : "no");

  if (args.fetch_first_message && fetch_first_message(session, messages) != 0) {
    mailgmail_message_list_free(messages);
    goto cleanup;
  }

  mailgmail_message_list_free(messages);
  status = 0;

 cleanup:
  mailgmail_free(session);
  return status;
}

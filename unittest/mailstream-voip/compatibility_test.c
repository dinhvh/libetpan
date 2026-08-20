#include <libetpan/libetpan.h>

#include <assert.h>

int main(void)
{
  mailstream * (* cf_open)(const char *, int16_t, int);
  mailstream * (* cf_open_timeout)(const char *, int16_t, int, time_t);
  mailstream_low * (* low_cf_open)(const char *, int16_t, int);
  mailstream_low * (* low_cf_open_timeout)(const char *, int16_t, int,
      time_t);
  int (* imap_socket_connect)(mailimap *, const char *, uint16_t, int);
  int (* imap_ssl_connect)(mailimap *, const char *, uint16_t, int);
  int saved_voip_enabled;

  cf_open = mailstream_cfstream_open_voip;
  cf_open_timeout = mailstream_cfstream_open_voip_timeout;
  low_cf_open = mailstream_low_cfstream_open_voip;
  low_cf_open_timeout = mailstream_low_cfstream_open_voip_timeout;
  imap_socket_connect = mailimap_socket_connect_voip;
  imap_ssl_connect = mailimap_ssl_connect_voip;

  assert(cf_open != NULL);
  assert(cf_open_timeout != NULL);
  assert(low_cf_open != NULL);
  assert(low_cf_open_timeout != NULL);
  assert(imap_socket_connect != NULL);
  assert(imap_ssl_connect != NULL);

  saved_voip_enabled = mailstream_cfstream_voip_enabled;
  mailstream_cfstream_voip_enabled = 1;
  assert(mailstream_cfstream_voip_enabled == 1);
  mailstream_cfstream_voip_enabled = 0;
  assert(mailstream_cfstream_voip_enabled == 0);
  mailstream_cfstream_voip_enabled = saved_voip_enabled;

  return 0;
}

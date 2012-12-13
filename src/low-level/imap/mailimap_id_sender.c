#include "mailimap_id_sender.h"

#include "mailimap_sender.h"

static int mailimap_id_param_send(mailstream * fd, struct mailimap_id_param * param);
static int mailimap_id_params_list_send(mailstream * fd, struct mailimap_id_params_list * list);

int mailimap_id_send(mailstream * fd, struct mailimap_id_params_list * client_identification)
{
  int r;

  r = mailimap_token_send(fd, "ID");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_id_params_list_send(fd, client_identification);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

static int mailimap_id_params_list_send(mailstream * fd, struct mailimap_id_params_list * list)
{
  int r;
  
  r = mailimap_oparenth_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_struct_spaced_list_send(fd, list->idpa_list,
  				  (mailimap_struct_sender *)
				  mailimap_id_param_send);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_cparenth_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  return MAILIMAP_NO_ERROR;
}

static int mailimap_id_param_send(mailstream * fd, struct mailimap_id_param * param)
{
  int r;
  
  r = mailimap_astring_send(fd, param->idpa_name);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (param->idpa_value == NULL) {
    r = mailimap_token_send(fd, "NIL");
    if (r != MAILIMAP_NO_ERROR)
      return r;
  }
  else {
    r = mailimap_astring_send(fd, param->idpa_value);
    if (r != MAILIMAP_NO_ERROR)
      return r;
  }
  
  return MAILIMAP_NO_ERROR;
}

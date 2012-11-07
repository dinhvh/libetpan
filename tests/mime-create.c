#include <libetpan/libetpan.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static char * generate_boundary(const char * boundary_prefix);

static struct mailmime * get_text_part(const char * mime_type,
	const char * text, size_t length, int encoding_type);

static struct mailimf_fields * build_fields(void)
{
  struct mailimf_fields * fields;
  struct mailimf_field * f;
  clist * list;
  struct mailimf_from * from;
	struct mailimf_to * to;
  struct mailimf_mailbox * mb;
  struct mailimf_address * addr;
  struct mailimf_mailbox_list * mb_list;
  struct mailimf_address_list * addr_list;
  clist * fields_list;

  /* build headers */

  fields_list = clist_new();
  
  /* build header 'From' */
  
  list = clist_new();
  mb = mailimf_mailbox_new(strdup("DINH =?iso-8859-1?Q?Vi=EAt_Ho=E0?="),
    strdup("dinh.viet.hoa@foobaremail.com"));
  clist_append(list, mb);
  mb_list = mailimf_mailbox_list_new(list);
  
  from = mailimf_from_new(mb_list);
  
  f = mailimf_field_new(MAILIMF_FIELD_FROM,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    from, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL);

  clist_append(fields_list, f);
  
  /* build header To */

  list = clist_new();
  mb = mailimf_mailbox_new(strdup("DINH =?iso-8859-1?Q?Vi=EAt_Ho=E0?="),
    strdup("dinh.viet.hoa@foobaremail.com"));
  addr = mailimf_address_new(MAILIMF_ADDRESS_MAILBOX, mb, NULL);
  clist_append(list, addr);
  addr_list = mailimf_address_list_new(list);
  
  to = mailimf_to_new(addr_list);

  f = mailimf_field_new(MAILIMF_FIELD_TO,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, to, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL);
  
  clist_append(fields_list, f);
  
  fields = mailimf_fields_new(fields_list);
  
  return fields;
}

static struct mailmime *
	part_new_empty(struct mailmime_content * content,
	struct mailmime_fields * mime_fields,
	const char * boundary_prefix,
	int force_single)
{
	struct mailmime * build_info;
	clist * list;
	int r;
	int mime_type;

	list = NULL;

	if (force_single) {
		mime_type = MAILMIME_SINGLE;
	}
	else {
		switch (content->ct_type->tp_type) {
			case MAILMIME_TYPE_DISCRETE_TYPE:
			mime_type = MAILMIME_SINGLE;
			break;

			case MAILMIME_TYPE_COMPOSITE_TYPE:
			switch (content->ct_type->tp_data.tp_composite_type->ct_type) {
				case MAILMIME_COMPOSITE_TYPE_MULTIPART:
				mime_type = MAILMIME_MULTIPLE;
				break;

				case MAILMIME_COMPOSITE_TYPE_MESSAGE:
				if (strcasecmp(content->ct_subtype, "rfc822") == 0)
					mime_type = MAILMIME_MESSAGE;
				else
					mime_type = MAILMIME_SINGLE;
				break;

				default:
				goto err;
			}
			break;

			default:
			goto err;
		}
	}

	if (mime_type == MAILMIME_MULTIPLE) {
		char * attr_name;
		char * attr_value;
		struct mailmime_parameter * param;
		clist * parameters;
		char * boundary;

		list = clist_new();
		if (list == NULL)
			goto err;

		attr_name = strdup("boundary");
		boundary = generate_boundary(boundary_prefix);
		attr_value = boundary;
		if (attr_name == NULL) {
			free(attr_name);
			goto free_list;
		}

		param = mailmime_parameter_new(attr_name, attr_value);
		if (param == NULL) {
			free(attr_value);
			free(attr_name);
			goto free_list;
		}

		if (content->ct_parameters == NULL) {
			parameters = clist_new();
			if (parameters == NULL) {
				mailmime_parameter_free(param);
				goto free_list;
			}
		}
		else
			parameters = content->ct_parameters;

		r = clist_append(parameters, param);
		if (r != 0) {
			clist_free(parameters);
			mailmime_parameter_free(param);
			goto free_list;
		}

		if (content->ct_parameters == NULL)
			content->ct_parameters = parameters;
	}

	build_info = mailmime_new(mime_type,
		NULL, 0, mime_fields, content,
		NULL, NULL, NULL, list,
		NULL, NULL);
	if (build_info == NULL) {
		clist_free(list);
		return NULL;
	}

	return build_info;

	free_list:
	clist_free(list);
	err:
	return NULL;
}

static struct mailmime * get_text_part(const char * mime_type,
								const char * text, size_t length, int encoding_type)
{
	struct mailmime_fields * mime_fields;
	struct mailmime * mime;
	struct mailmime_content * content;
	struct mailmime_parameter * param;
	struct mailmime_disposition * disposition;
	struct mailmime_mechanism * encoding;
    
	encoding = mailmime_mechanism_new(encoding_type, NULL);
	disposition = mailmime_disposition_new_with_data(MAILMIME_DISPOSITION_TYPE_INLINE,
		NULL, NULL, NULL, NULL, (size_t) -1);
	mime_fields = mailmime_fields_new_with_data(encoding,
		NULL, NULL, disposition, NULL);

	content = mailmime_content_new_with_str(mime_type);
	param = mailmime_param_new_with_data("charset", "utf-8");
	clist_append(content->ct_parameters, param);
	mime = part_new_empty(content, mime_fields, NULL, 1);
	mailmime_set_body_text(mime, (char *) text, length);
	
	return mime;
}

#define TEXT "You'll find a file as attachment"

static struct mailmime * get_plain_text_part(void)
{
  int mechanism;

	mechanism = MAILMIME_MECHANISM_QUOTED_PRINTABLE;
	return get_text_part("text/plain", TEXT, sizeof(TEXT) - 1, mechanism);
}

static struct mailmime * get_file_part(const char * filename, const char * mime_type,
                                       const char * text, size_t length)
{
	char * disposition_name;
	int encoding_type;
	struct mailmime_disposition * disposition;
	struct mailmime_mechanism * encoding;
	struct mailmime_content * content;
	struct mailmime * mime;
	struct mailmime_fields * mime_fields;
	
	disposition_name = NULL;
	if (filename != NULL) {
		disposition_name = strdup(filename);
	}
	disposition = mailmime_disposition_new_with_data(MAILMIME_DISPOSITION_TYPE_ATTACHMENT,
		disposition_name, NULL, NULL, NULL, (size_t) -1);
	content = mailmime_content_new_with_str(mime_type);
	
	encoding_type = MAILMIME_MECHANISM_BASE64;
	encoding = mailmime_mechanism_new(encoding_type, NULL);
	mime_fields = mailmime_fields_new_with_data(encoding,
		NULL, NULL, disposition, NULL);
	mime = part_new_empty(content, mime_fields, NULL, 1);
	mailmime_set_body_text(mime, (char *) text, length);
	
	return mime;
}

#define FILEDATA "SOME-IMAGE-DATA"

static struct mailmime * get_sample_file_part(void)
{
	struct mailmime * part;
	
	part = get_file_part("file-data.jpg", "image/jpeg",
		FILEDATA, sizeof(FILEDATA) - 1);

	return part;
}

#define MAX_MESSAGE_ID 512

static char * generate_boundary(const char * boundary_prefix)
{
    char id[MAX_MESSAGE_ID];
    time_t now;
    char name[MAX_MESSAGE_ID];
    long value;
    
    now = time(NULL);
    value = random();
    
    gethostname(name, MAX_MESSAGE_ID);
    
    if (boundary_prefix == NULL)
        boundary_prefix = "";
    
    snprintf(id, MAX_MESSAGE_ID, "%s%lx_%lx_%x", boundary_prefix, now, value, getpid());
    
    return strdup(id);
}

static struct mailmime * part_multiple_new(const char * type, const char * boundary_prefix)
{
    struct mailmime_fields * mime_fields;
    struct mailmime_content * content;
    struct mailmime * mp;
    
    mime_fields = mailmime_fields_new_empty();
    if (mime_fields == NULL)
        goto err;
    
    content = mailmime_content_new_with_str(type);
    if (content == NULL)
        goto free_fields;
    
    mp = part_new_empty(content, mime_fields, boundary_prefix, 0);
    if (mp == NULL)
        goto free_content;
    
    return mp;
    
free_content:
    mailmime_content_free(content);
free_fields:
    mailmime_fields_free(mime_fields);
err:
    return NULL;
}

static struct mailmime * get_multipart_mixed(const char * boundary_prefix)
{
	struct mailmime * mime;
	
	mime = part_multiple_new("multipart/mixed", boundary_prefix);
	
	return mime;
}

int main(int argc, char ** argv)
{
	struct mailmime * msg_mime;
  struct mailmime * mime;
	struct mailmime * submime;
  struct mailimf_fields * fields;
	int col;
	
	msg_mime = mailmime_new_message_data(NULL);
	fields = build_fields();
	mailmime_set_imf_fields(msg_mime, fields);
	
	mime = get_multipart_mixed(NULL);
  
  submime = get_plain_text_part();
	mailmime_smart_add_part(mime, submime);
  submime = get_sample_file_part();
	mailmime_smart_add_part(mime, submime);
  
  mailmime_add_part(msg_mime, mime);

	col = 0;
	mailmime_write_file(stdout, &col, mime);

	mailmime_free(msg_mime);

	exit(0);
}

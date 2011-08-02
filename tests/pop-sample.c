#include <libetpan/libetpan.h>
#include <stdlib.h>
#include <sys/stat.h>

static void check_error(int r, char * msg)
{
	if (r == MAILPOP3_NO_ERROR)
		return;
	
	fprintf(stderr, "%s\n", msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char ** argv)
{
	mailpop3 * pop3;
	int r;
	carray * list;
	unsigned int i;
	
	if (argc < 3) {
		fprintf(stderr, "syntax: pop-sample [gmail-email-address] [gmail-password]\n");
		exit(EXIT_FAILURE);
	}
	
	mkdir("download", 0700);
	
	pop3 = mailpop3_new(0, NULL);
	r = mailpop3_ssl_connect(pop3, "pop.gmail.com", 995);
	check_error(r, "connect failed");
	
	r = mailpop3_user(pop3, argv[1]);
	check_error(r, "user failed");
	
	r = mailpop3_pass(pop3, argv[2]);
	check_error(r, "pass failed");
	
	r = mailpop3_list(pop3, &list);
	check_error(r, "list failed");
	
	for(i = 0 ; i < carray_count(list) ; i ++) {
		struct mailpop3_msg_info * info;
		char * msg_content;
		size_t msg_size;
		FILE * f;
		char filename[512];
		struct stat stat_info;
		
		info = carray_get(list, i);
		
		if (info->msg_uidl == NULL) {
			continue;
		}
		
		snprintf(filename, sizeof(filename), "download/%s.eml", info->msg_uidl);
		r = stat(filename, &stat_info);
		if (r == 0) {
			printf("already fetched %u %s\n", info->msg_index, info->msg_uidl);
			continue;
		}
		
		r = mailpop3_retr(pop3, info->msg_index, &msg_content, &msg_size);
		check_error(r, "get failed");
		
		f = fopen(filename, "w");
		fwrite(msg_content, 1, msg_size, f);
		fclose(f);
		mailpop3_retr_free(msg_content);
		
		if (info->msg_uidl != NULL) {
			printf("fetched %u %s\n", info->msg_index, info->msg_uidl);
		}
		else {
			printf("fetched %u\n", info->msg_index);
		}
	}
	
	mailpop3_quit(pop3);
	mailpop3_free(pop3);
	
	exit(EXIT_SUCCESS);
}

#if defined(ENABLE_WWW)

#include <microhttpd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "bbs.h"

extern struct bbs_config conf;

int www_handler(void * cls, struct MHD_Connection * connection, const char * url, const char * method, const char * version, const char * upload_data, size_t * upload_data_size, void ** ptr) {
	struct MHD_Response *response;
	
	int ret;
	char *page;
	char buffer[4096];
	struct stat s;
	char *header;
	char *footer;
	char *whole_page;
	FILE *fptr;
	
	snprintf(buffer, 4096, "%s/header.tpl", conf.www_path);
	
	header = NULL;
	
	if (stat(buffer, &s) == 0) {
		header = (char *)malloc(s.st_size + 1);
		if (header == NULL) {
			return MHD_NO;
		}
		fptr = fopen(buffer, "r");
		if (fptr) {
			fread(header, s.st_size, 1, fptr);
			fclose(fptr);
		} else {
			free(header);
			header = NULL;
		}
	}
	
	if (header == NULL) {
		header = (char *)malloc(strlen(conf.bbs_name) * 2 + 61);
		if (header == NULL) {
			return MHD_NO;
		}
		sprintf(header, "<HTML>\n<HEAD>\n<TITLE>%s</TITLE>\n</HEAD>\n<BODY>\n<H1>%s</H1><HR />", conf.bbs_name, conf.bbs_name);
	}
	
	snprintf(buffer, 4096, "%s/footer.tpl", conf.www_path);
	
	footer = NULL;
	
	if (stat(buffer, &s) == 0) {
		footer = (char *)malloc(s.st_size + 1);
		if (footer == NULL) {
			free(header);
			return MHD_NO;
		}
		fptr = fopen(buffer, "r");
		if (fptr) {
			fread(footer, s.st_size, 1, fptr);
			fclose(fptr);
		} else {
			free(footer);
			footer = NULL;
		}
	}
	
	if (footer == NULL) {
		footer = (char *)malloc(43);
		if (footer == NULL) {
			free(header);
			return MHD_NO;
		}
		sprintf(footer, "<HR />Powered by Magicka BBS</BODY></HTML>");
	}
	
	if (strcmp(method, "GET") == 0) {
		if (strcasecmp(url, "/") == 0) {

			snprintf(buffer, 4096, "%s/index.tpl", conf.www_path);
			
			page = NULL;
			
			if (stat(buffer, &s) == 0) {
				page = (char *)malloc(s.st_size + 1);
				if (page == NULL) {
					free(header);
					free(footer);
					return MHD_NO;
				}
				fptr = fopen(buffer, "r");
				if (fptr) {
					fread(page, s.st_size, 1, fptr);
					fclose(fptr);
				} else {
					free(page);
					page = NULL;
				}
			}
			
			if (page == NULL) {
				page = (char *)malloc(16);
				if (page == NULL) {
					free(header);
					free(footer);					
					return MHD_NO;
				}
				sprintf(page, "Missing Content");
			}
				
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
			sprintf(whole_page, "%s%s%s", header, page, footer);
		} else if (strncasecmp(url, "/static/", 8) == 0) {
			// sanatize path
			
			// load file
		} else {
			free(header);
			free(footer);
			return MHD_NO;
		}
	} else if (strcmp(method, "POST") == 0) {
		free(header);
		free(footer);		
		return MHD_NO;
	}
	
	response = MHD_create_response_from_buffer (strlen (whole_page), (void*) whole_page, MHD_RESPMEM_PERSISTENT);
	
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	free(whole_page);
	free(page);
	free(header);
	free(footer);

	return ret;
}
#endif

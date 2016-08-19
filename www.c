#if defined(ENABLE_WWW)

#include <microhttpd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <b64/cdecode.h>
#include "bbs.h"

extern struct bbs_config conf;

struct mime_type {
	char *ext;
	char *mime;
};

static struct mime_type **mime_types;
static int mime_types_count;

void www_init() {
	FILE *fptr;
	char buffer[4096];
	int i;
	
	mime_types_count = 0;
	
	sprintf(buffer, "%s/mime.types", conf.www_path);
	
	fptr = fopen(buffer, "r");
	if (!fptr) {
		return;
	}
	fgets(buffer, 4096, fptr);
	while (!feof(fptr)) {
		chomp(buffer);
		
		for (i=0;i<strlen(buffer);i++) {
			if (buffer[i] == ' ') {
				buffer[i] = '\0';
				if (mime_types_count == 0) {
					mime_types = (struct mime_type **)malloc(sizeof(struct mime_type *));
				} else {
					mime_types = (struct mime_type **)realloc(mime_types, sizeof(struct mime_type *) * (mime_types_count + 1));
				}
				
				mime_types[mime_types_count] = (struct mime_type *)malloc(sizeof(struct mime_type));
				
				mime_types[mime_types_count]->mime = strdup(buffer);
				mime_types[mime_types_count]->ext = strdup(&buffer[i+1]);
				
				mime_types_count++;
				break;
			}
		}
		
		fgets(buffer, 4096, fptr);
	}
	
	fclose(fptr);
}

char *www_get_mime_type(const char *extension) {
	int i;
	static char default_mime_type[] = "application/octet-stream";
	
	
	for (i=0;i<mime_types_count;i++) {
		if (strcasecmp(extension, mime_types[i]->ext) == 0) {
			return mime_types[i]->mime;
		}
	}
	return default_mime_type;
}

int www_401(char *header, char *footer, struct MHD_Connection * connection) {
	char buffer[4096];
	char *page;
	struct stat s;
	char *whole_page;
	struct MHD_Response *response;
	int ret;
	FILE *fptr;
		
	snprintf(buffer, 4096, "%s/401.tpl", conf.www_path);
			
	page = NULL;
			
	if (stat(buffer, &s) == 0) {
		page = (char *)malloc(s.st_size + 1);
		if (page == NULL) {
			return -1;
		}
		memset(page, 0, s.st_size + 1);
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
			return -1;
		}
		sprintf(page, "Missing Content");
	}
				
	whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
	sprintf(whole_page, "%s%s%s", header, page, footer);

	response = MHD_create_response_from_buffer (strlen(whole_page), (void*)whole_page, MHD_RESPMEM_PERSISTENT);
	
	MHD_add_response_header(response, "WWW-Authenticate", "Basic realm=\"BBS Area\"");
	
	ret = MHD_queue_response (connection, 401, response);
	MHD_destroy_response (response);
	free(whole_page);
	free(page);
		
	return 0;
}

int www_404(char *header, char *footer, struct MHD_Connection * connection) {
	char buffer[4096];
	char *page;
	struct stat s;
	char *whole_page;
	struct MHD_Response *response;
	int ret;
	FILE *fptr;
		
	snprintf(buffer, 4096, "%s/404.tpl", conf.www_path);
			
	page = NULL;
			
	if (stat(buffer, &s) == 0) {
		page = (char *)malloc(s.st_size + 1);
		if (page == NULL) {
			return -1;
		}
		memset(page, 0, s.st_size + 1);
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
			return -1;
		}
		sprintf(page, "Missing Content");
	}
				
	whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
	sprintf(whole_page, "%s%s%s", header, page, footer);

	response = MHD_create_response_from_buffer (strlen(whole_page), (void*)whole_page, MHD_RESPMEM_PERSISTENT);
	
	ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
	MHD_destroy_response (response);
	free(whole_page);
	free(page);
		
	return 0;
}

int www_403(char *header, char *footer, struct MHD_Connection * connection) {
	char buffer[4096];
	char *page;
	struct stat s;
	char *whole_page;
	struct MHD_Response *response;
	int ret;
	FILE *fptr;
	
	snprintf(buffer, 4096, "%s/403.tpl", conf.www_path);
			
	page = NULL;
			
	if (stat(buffer, &s) == 0) {
		page = (char *)malloc(s.st_size + 1);
		if (page == NULL) {
			return -1;
		}
		memset(page, 0, s.st_size + 1);
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
			return -1;
		}
		sprintf(page, "Missing Content");
	}
				
	whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
	sprintf(whole_page, "%s%s%s", header, page, footer);

	response = MHD_create_response_from_buffer (strlen(whole_page), (void*)whole_page, MHD_RESPMEM_PERSISTENT);
	
	ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
	MHD_destroy_response (response);
	free(whole_page);
	free(page);
		
	return 0;
}

struct user_record *www_auth_ok(struct MHD_Connection *connection, const char *url) {
	char *user_password = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
	base64_decodestate state;
	char decoded_pass[34];
	int len;
	char *username;
	char *password;
	int i;
	
	if (user_password == NULL) {
		return NULL;
	}
	
	if (strncasecmp(user_password, "basic ", 6) == 0) {
		if (strlen(&user_password[6]) <= 48) {
			base64_init_decodestate(&state);
			len = base64_decode_block(&user_password[6], strlen(&user_password[6]), decoded_pass, &state);
			decoded_pass[len] = '\0';
			
			username = decoded_pass;
			for (i=0;i<strlen(decoded_pass);i++) {
				if (decoded_pass[i] == ':') {
					decoded_pass[i] = '\0';
					password = &decoded_pass[i+1];
					break;
				}
			}
			return check_user_pass(username, password);
		}
	}
	
	return NULL;
}

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
	char *mime;
	int i;
	int fno;
	const char *url_ = url;
	struct user_record *user;


	
	static int apointer;
	
	if (&apointer != *ptr) {
		*ptr = &apointer;
		return MHD_YES;
	}
	*ptr = NULL;
	
	snprintf(buffer, 4096, "%s/header.tpl", conf.www_path);
	
	header = NULL;
	
	if (stat(buffer, &s) == 0) {
		header = (char *)malloc(s.st_size + 1);
		if (header == NULL) {
			return MHD_NO;
		}
		memset(header, 0, s.st_size + 1);
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
		memset(footer, 0, s.st_size + 1);
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
				memset(page, 0, s.st_size + 1);
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
		} else if (strcasecmp(url, "/email/") == 0 || strcasecmp(url, "/email") == 0) {
			user = www_auth_ok(connection, url_);
			
			if (user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}
			page = www_email_summary(user);
			if (page == NULL) {
				free(header);
				free(footer);
				return MHD_NO;		
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);

			sprintf(whole_page, "%s%s%s", header, page, footer);
		} else if (strncasecmp(url, "/email/", 7) == 0) {
			user = www_auth_ok(connection, url_);
			
			if (user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}
			page = www_email_display(user, atoi(&url[7]));
			if (page == NULL) {
				free(header);
				free(footer);
				return MHD_NO;		
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
			sprintf(whole_page, "%s%s%s", header, page, footer);			
		} else if (strncasecmp(url, "/static/", 8) == 0) {
			// sanatize path
			if (strstr(url, "/..") != NULL) {
				return MHD_NO;
			}
			// get mimetype
			for (i=strlen(url);i>0;--i) {
				if (url[i] == '.') {
					mime = www_get_mime_type(&url[i+1]);
					break;
				}
			}
			// load file
			
			sprintf(buffer, "%s%s", conf.www_path, url);
			if (stat(buffer, &s) == 0 && S_ISREG(s.st_mode)) {
				fno = open(buffer, O_RDONLY);
				if (fno != -1) {
					response = MHD_create_response_from_fd(s.st_size, fno);
					MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, mime);
					ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
					MHD_destroy_response (response);
					free(header);
					free(footer);
					return MHD_YES;				
				} else {
					if (www_403(header, footer, connection) != 0) {
						free(header);
						free(footer);
						return MHD_NO;	
					}
					free(header);
					free(footer);
					return MHD_YES;	
				}
			} else {
				if (www_404(header, footer, connection) != 0) {
					free(header);
					free(footer);
					return MHD_NO;	
				}
				free(header);
				free(footer);
				return MHD_YES;	
			}
			
		} else {
			if (www_404(header, footer, connection) != 0) {
				free(header);
				free(footer);
				return MHD_NO;	
			}
			free(header);
			free(footer);
			return MHD_YES;	
		}
	} else if (strcmp(method, "POST") == 0) {
		free(header);
		free(footer);		
		return MHD_NO;
	}
	response = MHD_create_response_from_buffer (strlen (whole_page), (void*) whole_page, MHD_RESPMEM_PERSISTENT);
	
	MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
	
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	free(page);
	free(header);
	free(footer);

	return ret;
}
#endif

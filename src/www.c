#if defined(ENABLE_WWW)

#include <microhttpd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <b64/cdecode.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
#include <netinet/in.h>
#endif
#include "bbs.h"

#define GET 1
#define POST 2

#define POSTBUFFERSIZE 65536

extern struct bbs_config conf;
extern char *ipaddress;

struct mime_type {
	char *ext;
	char *mime;
};

static struct mime_type **mime_types;
static int mime_types_count;

struct connection_info_s {
	int connection_type;
	struct user_record *user;
	char *url;
	char **keys;
	char **values;
	int count;
	struct MHD_PostProcessor *pp;
};

void *www_logger(void * cls, const char * uri, struct MHD_Connection *con) {
	struct sockaddr *so = (struct sockaddr *)MHD_get_connection_info(con, MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
	if (so->sa_family == AF_INET) {
		ipaddress = (char *)malloc(INET_ADDRSTRLEN + 1);
		inet_ntop(AF_INET, &((struct sockaddr_in *)so)->sin_addr, ipaddress, INET_ADDRSTRLEN);
	} else if (so->sa_family == AF_INET6) {
		ipaddress = (char *)malloc(INET6_ADDRSTRLEN + 1);
		inet_ntop(AF_INET6, &((struct sockaddr_in6 *)so)->sin6_addr, ipaddress, INET6_ADDRSTRLEN);
	}
	dolog("%s", uri);
	free(ipaddress);
	ipaddress = NULL;	
	return NULL;
}

void www_request_completed(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe) {
	struct connection_info_s *con_info = *con_cls;
	int i;
	if (con_info == NULL) {
		return;
	}
	
	if (con_info->connection_type == POST) {

		if (con_info->count > 0) {
			for (i=0;i<con_info->count;i++) {
				free(con_info->values[i]);
				free(con_info->keys[i]);
			}
			free(con_info->values);
			free(con_info->keys);
		}

		if (con_info->pp != NULL) {	
			MHD_destroy_post_processor(con_info->pp);
		}
	}
	if (con_info->user != NULL) {
		free(con_info->user->loginname);
		free(con_info->user->password);
		free(con_info->user->firstname);
		free(con_info->user->lastname);
		free(con_info->user->email);
		free(con_info->user->location);
		free(con_info->user->sec_info);
		free(con_info->user->signature);
		free(con_info->user);
	}	
	
	free(con_info->url);
	free(con_info);
}

static int iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key, const char *filename, const char *content_type, const char *transfer_encoding, const char *data, uint64_t off, size_t size) {
	struct connection_info_s *con_info = coninfo_cls;

	int i;

	if (size == 0) {
		return MHD_NO;
	}


	if (con_info != NULL) {
		if (con_info->connection_type == POST) {
			for (i=0;i<con_info->count;i++) {
				if (strcmp(con_info->keys[i], key) == 0) {
					con_info->values[i] = (char *)realloc(con_info->values[i], strlen(con_info->values[i]) + size + 1);
					strcat(con_info->values[i], data);
					return MHD_YES;
				}
			}
			
			if (con_info->count == 0) {
				con_info->keys = (char **)malloc(sizeof(char *));
				con_info->values = (char **)malloc(sizeof(char *));
			} else {
				con_info->keys = (char **)realloc(con_info->keys, sizeof(char *) * (con_info->count + 1));
				con_info->values = (char **)realloc(con_info->values, sizeof(char *) * (con_info->count + 1));				
			}
			con_info->keys[con_info->count] = strdup(key);
			con_info->values[con_info->count] = strdup(data);
			con_info->count++;
			return MHD_YES;
		} else {
			return MHD_NO;
		}
	}

	return MHD_NO;
}

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
	char *page, *page_tmp;
	struct stat s;
	char *whole_page;
	struct MHD_Response *response;
	int ret;
	FILE *fptr;
		
	snprintf(buffer, 4096, "%s/401.tpl", conf.www_path);
			
	page_tmp = NULL;
			
	if (stat(buffer, &s) == 0) {
		page_tmp = (char *)malloc(s.st_size + 1);
		if (page_tmp == NULL) {
			return -1;
		}
		memset(page_tmp, 0, s.st_size + 1);
		fptr = fopen(buffer, "r");
		if (fptr) {
			fread(page_tmp, s.st_size, 1, fptr);
			fclose(fptr);
		} else {
			free(page_tmp);
			page_tmp = NULL;
		}
	}
			
	if (page_tmp == NULL) {
		page_tmp = (char *)malloc(16);
		if (page_tmp == NULL) {
			return -1;
		}
		sprintf(page_tmp, "Missing Content");
	}

	page = str_replace(page_tmp, "@@WWW_URL@@", conf.www_url);
	free(page_tmp);

	whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
	sprintf(whole_page, "%s%s%s", header, page, footer);

	response = MHD_create_response_from_buffer (strlen(whole_page), (void*)whole_page, MHD_RESPMEM_MUST_FREE);
	
	MHD_add_response_header(response, "WWW-Authenticate", "Basic realm=\"BBS Area\"");
	
	ret = MHD_queue_response (connection, 401, response);
	MHD_destroy_response (response);
	free(page);
		
	return 0;
}

int www_404(char *header, char *footer, struct MHD_Connection * connection) {
	char buffer[4096];
	char *page, *page_tmp;
	struct stat s;
	char *whole_page;
	struct MHD_Response *response;
	int ret;
	FILE *fptr;
		
	snprintf(buffer, 4096, "%s/404.tpl", conf.www_path);
			
	page_tmp = NULL;
			
	if (stat(buffer, &s) == 0) {
		page_tmp = (char *)malloc(s.st_size + 1);
		if (page_tmp == NULL) {
			return -1;
		}
		memset(page_tmp, 0, s.st_size + 1);
		fptr = fopen(buffer, "r");
		if (fptr) {
			fread(page_tmp, s.st_size, 1, fptr);
			fclose(fptr);
		} else {
			free(page_tmp);
			page = NULL;
		}
	}
			
	if (page_tmp == NULL) {
		page_tmp = (char *)malloc(16);
		if (page_tmp == NULL) {
			return -1;
		}
		sprintf(page_tmp, "Missing Content");
	}

	page = str_replace(page_tmp, "@@WWW_URL@@", conf.www_url);
	free(page_tmp);

	whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
	sprintf(whole_page, "%s%s%s", header, page, footer);

	response = MHD_create_response_from_buffer (strlen(whole_page), (void*)whole_page, MHD_RESPMEM_MUST_FREE);
	
	ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
	MHD_destroy_response (response);
	free(page);
		
	return 0;
}

int www_403(char *header, char *footer, struct MHD_Connection * connection) {
	char buffer[4096];
	char *page, *page_tmp;
	struct stat s;
	char *whole_page;
	struct MHD_Response *response;
	int ret;
	FILE *fptr;
	
	snprintf(buffer, 4096, "%s/403.tpl", conf.www_path);
			
	page_tmp = NULL;
			
	if (stat(buffer, &s) == 0) {
		page_tmp = (char *)malloc(s.st_size + 1);
		if (page_tmp == NULL) {
			return -1;
		}
		memset(page_tmp, 0, s.st_size + 1);
		fptr = fopen(buffer, "r");
		if (fptr) {
			fread(page_tmp, s.st_size, 1, fptr);
			fclose(fptr);
		} else {
			free(page_tmp);
			page_tmp = NULL;
		}
	}
			
	if (page_tmp == NULL) {
		page_tmp = (char *)malloc(16);
		if (page_tmp == NULL) {
			return -1;
		}
		sprintf(page_tmp, "Missing Content");
	}

	page = str_replace(page_tmp, "@@WWW_URL@@", conf.www_url);
	free(page_tmp);

	whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
	sprintf(whole_page, "%s%s%s", header, page, footer);

	response = MHD_create_response_from_buffer (strlen(whole_page), (void*)whole_page, MHD_RESPMEM_MUST_FREE);
	
	ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
	MHD_destroy_response (response);
	free(page);
		
	return 0;
}

struct user_record *www_auth_ok(struct MHD_Connection *connection, const char *url) {
	char *ptr = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
	char *user_password;
	base64_decodestate state;
	char decoded_pass[34];
	int len;
	char *username;
	char *password;
	int i;
	struct user_record *u;
	
	if (ptr == NULL) {
		return NULL;
	}
	
	user_password = strdup(ptr);
	
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
			u = check_user_pass(username, password);
			free(user_password);
			
			return u;
		}
	}
	free(user_password);
	return NULL;
}

int www_handler(void * cls, struct MHD_Connection * connection, const char * url, const char * method, const char * version, const char * upload_data, size_t * upload_data_size, void ** ptr) {
	struct MHD_Response *response;
	
	int ret;
	char *page, *page_tmp;
	char buffer[4096];
	struct stat s;
	char *header, *header_tmp;
	char *footer, *footer_tmp;
	char *whole_page;
	FILE *fptr;
	char *mime;
	int i;
	int fno;
	const char *url_ = url;
	char *subj, *to, *body;
	struct connection_info_s *con_inf;
	int conference, area, msg;
	char *url_copy;
	char *aptr;
	const char *val;
	int skip;
	char *replyid;
	
	if (strcmp(method, "GET") == 0) {
		if (*ptr == NULL) {
			con_inf = (struct connection_info_s *)malloc(sizeof(struct connection_info_s));
			if (!con_inf) {
				return MHD_NO;
			}
			con_inf->connection_type = GET;
			con_inf->user = NULL;
			con_inf->count = 0;
			con_inf->url = strdup(url);
			con_inf->pp = NULL;
			*ptr = con_inf;
			return MHD_YES;
		}
	} else if (strcmp(method, "POST") == 0) {
		if (*ptr == NULL) {
			con_inf = (struct connection_info_s *)malloc(sizeof(struct connection_info_s));
			if (!con_inf) {
				return MHD_NO;
			}
			con_inf->connection_type = POST;
			con_inf->user = NULL;
			con_inf->count = 0;
			con_inf->url = strdup(url);
			con_inf->pp = NULL;
			*ptr = con_inf;
			return MHD_YES;
		}
	} else {
		return MHD_NO;
	}
	
	con_inf = *ptr;
	
	snprintf(buffer, 4096, "%s/header.tpl", conf.www_path);
	
	header_tmp = NULL;
	
	if (stat(buffer, &s) == 0) {
		header_tmp = (char *)malloc(s.st_size + 1);
		if (header_tmp == NULL) {
			return MHD_NO;
		}
		memset(header_tmp, 0, s.st_size + 1);
		fptr = fopen(buffer, "r");
		if (fptr) {
			fread(header_tmp, s.st_size, 1, fptr);
			fclose(fptr);
		} else {
			free(header_tmp);
			header_tmp = NULL;
		}
	}
	
	if (header_tmp == NULL) {
		header_tmp = (char *)malloc(strlen(conf.bbs_name) * 2 + 61);
		if (header_tmp == NULL) {
			return MHD_NO;
		}
		sprintf(header_tmp, "<HTML>\n<HEAD>\n<TITLE>%s</TITLE>\n</HEAD>\n<BODY>\n<H1>%s</H1><HR />", conf.bbs_name, conf.bbs_name);
	}
	
	header = str_replace(header_tmp, "@@WWW_URL@@", conf.www_url);
	free(header_tmp);

	snprintf(buffer, 4096, "%s/footer.tpl", conf.www_path);
	
	footer = NULL;
	
	if (stat(buffer, &s) == 0) {
		footer_tmp = (char *)malloc(s.st_size + 1);
		if (footer_tmp == NULL) {
			free(header);
			return MHD_NO;
		}
		memset(footer_tmp, 0, s.st_size + 1);
		fptr = fopen(buffer, "r");
		if (fptr) {
			fread(footer_tmp, s.st_size, 1, fptr);
			fclose(fptr);
		} else {
			free(footer_tmp);
			footer_tmp = NULL;
		}
	}
	
	if (footer_tmp == NULL) {
		footer_tmp = (char *)malloc(43);
		if (footer_tmp == NULL) {
			free(header);
			return MHD_NO;
		}
		sprintf(footer_tmp, "<HR />Powered by Magicka BBS</BODY></HTML>");
	}

	footer = str_replace(footer_tmp, "@@WWW_URL@@", conf.www_url);
	free(footer_tmp);

	if (strcmp(method, "GET") == 0) {
		if (strcasecmp(url, "/") == 0) {

			snprintf(buffer, 4096, "%s/index.tpl", conf.www_path);
			
			page_tmp = NULL;
			
			if (stat(buffer, &s) == 0) {
				page_tmp = (char *)malloc(s.st_size + 1);
				if (page_tmp == NULL) {
					free(header);
					free(footer);
					return MHD_NO;
				}
				memset(page_tmp, 0, s.st_size + 1);
				fptr = fopen(buffer, "r");
				if (fptr) {
					fread(page_tmp, s.st_size, 1, fptr);
					fclose(fptr);
				} else {
					free(page_tmp);
					page_tmp = NULL;
				}
			}
			
			if (page_tmp == NULL) {
				page_tmp = (char *)malloc(16);
				if (page_tmp == NULL) {
					free(header);
					free(footer);					
					return MHD_NO;
				}
				sprintf(page_tmp, "Missing Content");
			}
		
			page = str_replace(page_tmp, "@@WWW_URL@@", conf.www_url);
			free(page_tmp);

			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
			sprintf(whole_page, "%s%s%s", header, page, footer);
		} else if (strcasecmp(url, "/last10/") == 0 || strcasecmp(url, "/last10") == 0) {
			page = www_last10();
			if (page == NULL) {
				free(header);
				free(footer);
				return MHD_NO;		
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);

			sprintf(whole_page, "%s%s%s", header, page, footer);
		} else if (strcasecmp(url, "/email/") == 0 || strcasecmp(url, "/email") == 0) {
			con_inf->user = www_auth_ok(connection, url_);
			
			if (con_inf->user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}
			page = www_email_summary(con_inf->user);
			if (page == NULL) {
				free(header);
				free(footer);
				return MHD_NO;		
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);

			sprintf(whole_page, "%s%s%s", header, page, footer);
		} else if(strcasecmp(url, "/email/new") == 0) {
			con_inf->user = www_auth_ok(connection, url_);
			
			if (con_inf->user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}
			page = www_new_email();
			if (page == NULL) {
				free(header);
				free(footer);
				return MHD_NO;		
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
			sprintf(whole_page, "%s%s%s", header, page, footer);						
		} else if (strncasecmp(url, "/email/delete/", 14) == 0) {
			con_inf->user = www_auth_ok(connection, url_);
			
			if (con_inf->user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}
			
			if (!www_email_delete(con_inf->user, atoi(&url[14]))) {
				page = (char *)malloc(31);
				if (page == NULL) {
					free(header);
					free(footer);					
					return MHD_NO;
				}
				sprintf(page, "<h1>Error Deleting Email.</h1>");
			} else {
				page = (char *)malloc(24);
				if (page == NULL) {
					free(header);
					free(footer);					
					return MHD_NO;
				}
				sprintf(page, "<h1>Email Deleted!</h1>");
			}
			if (page == NULL) {
				free(header);
				free(footer);
				return MHD_NO;		
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
			sprintf(whole_page, "%s%s%s", header, page, footer);						
		} else if (strncasecmp(url, "/email/", 7) == 0) {
			con_inf->user = www_auth_ok(connection, url_);
			
			if (con_inf->user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}
			page = www_email_display(con_inf->user, atoi(&url[7]));
			if (page == NULL) {
				free(header);
				free(footer);
				return MHD_NO;		
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
			sprintf(whole_page, "%s%s%s", header, page, footer);
		} else if (strcasecmp(url, "/msgs/") == 0 || strcasecmp(url, "/msgs") == 0) {
			con_inf->user = www_auth_ok(connection, url_);
			
			if (con_inf->user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}
			page = www_msgs_arealist(con_inf->user);
			if (page == NULL) {
				free(header);
				free(footer);
				return MHD_NO;		
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
			sprintf(whole_page, "%s%s%s", header, page, footer);
		} else if (strncasecmp(url, "/msgs/new/", 10) == 0) {
			con_inf->user = www_auth_ok(connection, url_);
		
			if (con_inf->user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}			
			conference = -1;
			area = -1;
			url_copy = strdup(&url[10]);
	
			aptr = strtok(url_copy, "/");
			if (aptr != NULL) {
				conference = atoi(aptr);
				aptr = strtok(NULL, "/");
				if (aptr != NULL) {
					area = atoi(aptr);
				}
			}
			free(url_copy);
			
			if (area != -1 && conference != -1) {
				page = www_new_msg(con_inf->user, conference, area);
			} else {
				free(header);
				free(footer);
				return MHD_NO;	
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
			sprintf(whole_page, "%s%s%s", header, page, footer);			
		} else if (strncasecmp(url, "/msgs/", 6) == 0) {
			con_inf->user = www_auth_ok(connection, url_);
		
			if (con_inf->user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}			
			conference = -1;
			area = -1;
			msg = -1;
			url_copy = strdup(&url[6]);
	
			aptr = strtok(url_copy, "/");
			if (aptr != NULL) {
				conference = atoi(aptr);
				aptr = strtok(NULL, "/");
				if (aptr != NULL) {
					area = atoi(aptr);
					aptr = strtok(NULL, "/");
					if (aptr != NULL) {
						msg = atoi(aptr);
					}
				}
			}
			free(url_copy);
			
			val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "skip");
			
			if (val != NULL) {
				skip = atoi(val); 
			} else {
				skip = 0;
			}

			if (conference != -1 && area != -1 && msg == -1) {
				page = www_msgs_messagelist(con_inf->user, conference, area, skip);
			} else if (conference != -1 && area != -1 && msg != -1) {
				page = www_msgs_messageview(con_inf->user, conference, area, msg);
			}
			
			
			if (page == NULL) {
				if (www_403(header, footer, connection) != 0) {
					free(header);
					free(footer);
					return MHD_NO;	
				}
				free(header);
				free(footer);
				return MHD_YES;		
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
					return ret;				
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
		if (strcasecmp(url, "/email/") == 0 || strcasecmp(url, "/email") == 0) {
			con_inf->user = www_auth_ok(connection, url_);

			if (con_inf->user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}
		        if (con_inf->pp == NULL) {				
				con_inf->pp = MHD_create_post_processor(connection, POSTBUFFERSIZE, iterate_post, (void*) con_inf);   
			}	
			if (*upload_data_size != 0) {
				
				MHD_post_process (con_inf->pp, upload_data, *upload_data_size);
				*upload_data_size = 0;
          
				return MHD_YES;
			}
			subj = NULL;
			to = NULL;
			body = NULL;
			for (i=0;i<con_inf->count;i++) {
				if (strcmp(con_inf->keys[i], "recipient") == 0) {
					to = con_inf->values[i];
				} else if (strcmp(con_inf->keys[i], "subject") == 0) {
					subj = con_inf->values[i];
				} else if (strcmp(con_inf->keys[i], "body") == 0) {
					body = con_inf->values[i];
				}
			}
			if (!www_send_email(con_inf->user, to, subj, body)) {
				page = (char *)malloc(50);
				if (page == NULL) {
					free(header);
					free(footer);					
					return MHD_NO;
				}
				sprintf(page, "<h1>Error Sending Email (Check User Exists?)</h1>");
			} else {
				page = (char *)malloc(21);
				if (page == NULL) {
					free(header);
					free(footer);					
					return MHD_NO;
				}
				sprintf(page, "<h1>Email Sent!</h1>");
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
			sprintf(whole_page, "%s%s%s", header, page, footer);
	
		} else if (strcasecmp(url, "/msgs/") == 0 || strcasecmp(url, "/msgs") == 0) {
			con_inf->user = www_auth_ok(connection, url_);

			if (con_inf->user == NULL) {
				www_401(header, footer, connection);
				free(header);
				free(footer);
				return MHD_YES;		
			}
			if (con_inf->pp == NULL) {			
				con_inf->pp = MHD_create_post_processor(connection, POSTBUFFERSIZE, iterate_post, (void*) con_inf);   
			}
			
			if (*upload_data_size != 0) {
				MHD_post_process (con_inf->pp, upload_data, *upload_data_size);
				*upload_data_size = 0;
          
				return MHD_YES;
			}
			subj = NULL;
			to = NULL;
			body = NULL;
			replyid = NULL;
			conference = -1;
			area = -1;
			
			for (i=0;i<con_inf->count;i++) {
				if (strcmp(con_inf->keys[i], "recipient") == 0) {
					to = con_inf->values[i];
				} else if (strcmp(con_inf->keys[i], "subject") == 0) {
					subj = con_inf->values[i];
				} else if (strcmp(con_inf->keys[i], "body") == 0) {
					body = con_inf->values[i];
				} else if (strcmp(con_inf->keys[i], "conference") == 0) {
					conference = atoi(con_inf->values[i]);
				} else if (strcmp(con_inf->keys[i], "area") == 0) {
					area = atoi(con_inf->values[i]);
				} else if (strcmp(con_inf->keys[i], "replyid") == 0) {
					replyid = con_inf->values[i];
				}
			}

			if (!www_send_msg(con_inf->user, to, subj, conference, area, replyid, body)) {
				page = (char *)malloc(31);
				if (page == NULL) {
					free(header);
					free(footer);					
					return MHD_NO;
				}
				sprintf(page, "<h1>Error Sending Message</h1>");
			} else {
				page = (char *)malloc(23);
				if (page == NULL) {
					free(header);
					free(footer);					
					return MHD_NO;
				}
				sprintf(page, "<h1>Message Sent!</h1>");
			}
			whole_page = (char *)malloc(strlen(header) + strlen(page) + strlen(footer) + 1);
			
			sprintf(whole_page, "%s%s%s", header, page, footer);
	
			
		} else {
			free(header);
			free(footer);		
			return MHD_NO;
		}
	} else {
		free(header);
		free(footer);		
		return MHD_NO;		
	}
	response = MHD_create_response_from_buffer (strlen (whole_page), (void*) whole_page, MHD_RESPMEM_MUST_FREE);
	
	MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
	
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	free(page);
	free(header);
	free(footer);

	return ret;
}
#endif

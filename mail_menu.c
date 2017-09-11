#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <fcntl.h>
#include "jamlib/jam.h"
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"

extern struct bbs_config conf;
extern struct user_record *gUser;
extern int mynode;

time_t utc_to_local(time_t utc) {
	time_t local;
	struct tm date_time;

	localtime_r(&utc, &date_time);

	local = utc + date_time.tm_gmtoff;

	return local;
}


s_JamBase *open_jam_base(char *path) {
	int ret;
	s_JamBase *jb;

	ret = JAM_OpenMB((char *)path, &jb);

	if (ret != 0) {
		if (ret == JAM_IO_ERROR) {
			free(jb);
			ret = JAM_CreateMB((char *)path, 1, &jb);
			if (ret != 0) {
				free(jb);
				return NULL;
			}
		} else {
			free(jb);
			dolog("Got %d", ret);
			return NULL;
		}
	}
	return jb;
}

unsigned long generate_msgid() {
	
	char buffer[1024];
	time_t unixtime;

	unsigned long msgid;
	unsigned long lastid;
	FILE *fptr;

	snprintf(buffer, 1024, "%s/msgserial", conf.bbs_path);
	
	unixtime = time(NULL);

	fptr = fopen(buffer, "r+");
	if (fptr) {
		flock(fileno(fptr), LOCK_EX);
		fread(&lastid, sizeof(unsigned long), 1, fptr);	

		if (unixtime > lastid) {
			lastid = unixtime;
		} else {
			lastid++;
		}
		rewind(fptr);
		fwrite(&lastid, sizeof(unsigned long), 1, fptr);
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
	} else {
		fptr = fopen(buffer, "w");
		if (fptr) {
			lastid = unixtime;
			flock(fileno(fptr), LOCK_EX);
			fwrite(&lastid, sizeof(unsigned long), 1, fptr);
			flock(fileno(fptr), LOCK_UN);
			fclose(fptr);
		} else {
			lastid = unixtime;
			dolog("Unable to open message id log");
		}
	}
	sprintf(buffer, "%lX", lastid);
	return strtoul(&buffer[strlen(buffer) - 8], NULL, 16);
}

void free_message_headers(struct msg_headers *msghs) {
	int i;

	for (i=0;i<msghs->msg_count;i++) {
		free(msghs->msgs[i]->msg_h);
		if (msghs->msgs[i]->from != NULL) {
			free(msghs->msgs[i]->from);
		}
		if (msghs->msgs[i]->to != NULL) {
			free(msghs->msgs[i]->to);
		}
		if (msghs->msgs[i]->from != NULL) {
			free(msghs->msgs[i]->subject);
		}
		if (msghs->msgs[i]->oaddress != NULL) {
			free(msghs->msgs[i]->oaddress);
		}
		if (msghs->msgs[i]->daddress != NULL) {
			free(msghs->msgs[i]->daddress);
		}
		if (msghs->msgs[i]->msgid != NULL) {
			free(msghs->msgs[i]->msgid);
		}
		if (msghs->msgs[i]->replyid != NULL) {
			free(msghs->msgs[i]->replyid);
		}
	}
	if (msghs->msg_count > 0) {
		free(msghs->msgs);
	}
	free(msghs);
}

int msg_is_to(struct user_record *user, char *addressed_to, char *address, int type, int rn, int msgconf) {
	char *myname;
	struct fido_addr *dest;
	int j;
	if (rn) {
		myname = (char *)malloc(strlen(user->firstname) + strlen(user->lastname) + 2);
		sprintf(myname, "%s %s", user->firstname, user->lastname);
	} else {
		myname = strdup(user->loginname);
	}
	if (type == NETWORK_FIDO) {
		if (strcasecmp(myname, addressed_to) == 0) {
			dest = parse_fido_addr(address);
			if (conf.mail_conferences[msgconf]->fidoaddr->zone == dest->zone &&
				conf.mail_conferences[msgconf]->fidoaddr->net == dest->net &&
				conf.mail_conferences[msgconf]->fidoaddr->node == dest->node &&
				conf.mail_conferences[msgconf]->fidoaddr->point == dest->point) {
					free(dest);
					free(myname);
					return 1;
			}
			free(dest);
		}
		free(myname);
		return 0;
	} else {
		if (strcasecmp(myname, addressed_to) == 0) {
			free(myname);
			return 1;
		}
		free(myname);
		return 0;
	}
}

int msg_is_from(struct user_record *user, char *addressed_from, char *address, int type, int rn, int msgconf) {
	char *myname;
	struct fido_addr *orig;
	int j;
	if (rn) {
		myname = (char *)malloc(strlen(user->firstname) + strlen(user->lastname) + 2);
		sprintf(myname, "%s %s", user->firstname, user->lastname);
	} else {
		myname = strdup(user->loginname);
	}
	if (type == NETWORK_FIDO) {
		if (strcasecmp(myname, addressed_from) == 0) {
			orig = parse_fido_addr(address);
			if (conf.mail_conferences[msgconf]->fidoaddr->zone == orig->zone &&
				conf.mail_conferences[msgconf]->fidoaddr->net == orig->net &&
				conf.mail_conferences[msgconf]->fidoaddr->node == orig->node &&
				conf.mail_conferences[msgconf]->fidoaddr->point == orig->point) {
					free(orig);
					free(myname);
					return 1;
			}
			free(orig);
		}
		free(myname);
		return 0;
	} else {
		if (strcasecmp(myname, addressed_from) == 0) {
			free(myname);
			return 1;
		}
		free(myname);
		return 0;
	}
}

struct msg_headers *read_message_headers(int msgconf, int msgarea, struct user_record *user) {
	s_JamBase *jb;
	s_JamBaseHeader jbh;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;
	struct jam_msg *jamm;
	int to_us;
	int i;
	int z;
	int j;
	int k;

	struct fido_addr *dest;
	struct msg_headers *msghs = NULL;

	jb = open_jam_base(conf.mail_conferences[msgconf]->mail_areas[msgarea]->path);
	if (!jb) {
		dolog("Error opening JAM base.. %s", conf.mail_conferences[msgconf]->mail_areas[msgarea]->path);
		return NULL;
	}

	JAM_ReadMBHeader(jb, &jbh);

	if (jbh.ActiveMsgs > 0) {
		msghs = (struct msg_headers *)malloc(sizeof(struct msg_headers));
		msghs->msg_count = 0;
		k = 0;
		for (i=0;k < jbh.ActiveMsgs;i++) {

			memset(&jmh, 0, sizeof(s_JamMsgHeader));
			z = JAM_ReadMsgHeader(jb, i, &jmh, &jsp);
			if (z != 0) {
				dolog("Failed to read msg header: %d Erro %d", z, JAM_Errno(jb));
				continue;
			}

			if (jmh.Attribute & JAM_MSG_DELETED) {
				JAM_DelSubPacket(jsp);
				continue;
			}

			jamm = (struct jam_msg *)malloc(sizeof(struct jam_msg));

			jamm->msg_no = i;
			jamm->msg_h = (s_JamMsgHeader *)malloc(sizeof(s_JamMsgHeader));
			memcpy(jamm->msg_h, &jmh, sizeof(s_JamMsgHeader));
			jamm->from = NULL;
			jamm->to = NULL;
			jamm->subject = NULL;
			jamm->oaddress = NULL;
			jamm->daddress = NULL;
			jamm->msgid = NULL;
			jamm->replyid = NULL;

			for (z=0;z<jsp->NumFields;z++) {
				if (jsp->Fields[z]->LoID == JAMSFLD_SUBJECT) {
					jamm->subject = (char *)malloc(jsp->Fields[z]->DatLen + 1);
					memset(jamm->subject, 0, jsp->Fields[z]->DatLen + 1);
					memcpy(jamm->subject, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
				}
				if (jsp->Fields[z]->LoID == JAMSFLD_SENDERNAME) {
					jamm->from = (char *)malloc(jsp->Fields[z]->DatLen + 1);
					memset(jamm->from, 0, jsp->Fields[z]->DatLen + 1);
					memcpy(jamm->from, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
				}
				if (jsp->Fields[z]->LoID == JAMSFLD_RECVRNAME) {
					jamm->to = (char *)malloc(jsp->Fields[z]->DatLen + 1);
					memset(jamm->to, 0, jsp->Fields[z]->DatLen + 1);
					memcpy(jamm->to, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
				}
				if (jsp->Fields[z]->LoID == JAMSFLD_DADDRESS) {
					jamm->daddress = (char *)malloc(jsp->Fields[z]->DatLen + 1);
					memset(jamm->daddress, 0, jsp->Fields[z]->DatLen + 1);
					memcpy(jamm->daddress, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
				}
				if (jsp->Fields[z]->LoID == JAMSFLD_OADDRESS) {
					jamm->oaddress = (char *)malloc(jsp->Fields[z]->DatLen + 1);
					memset(jamm->oaddress, 0, jsp->Fields[z]->DatLen + 1);
					memcpy(jamm->oaddress, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
				}
				if (jsp->Fields[z]->LoID == JAMSFLD_MSGID) {
					jamm->msgid = (char *)malloc(jsp->Fields[z]->DatLen + 1);
					memset(jamm->msgid, 0, jsp->Fields[z]->DatLen + 1);
					memcpy(jamm->msgid, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
				}
				if (jsp->Fields[z]->LoID == JAMSFLD_REPLYID) {
					jamm->replyid = (char *)malloc(jsp->Fields[z]->DatLen + 1);
					memset(jamm->replyid, 0, jsp->Fields[z]->DatLen + 1);
					memcpy(jamm->replyid, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
				}
			}
			JAM_DelSubPacket(jsp);

			if (jamm->subject == NULL) {
				jamm->subject = strdup("(No Subject)");
			}

			if (jmh.Attribute & JAM_MSG_PRIVATE) {
				if (!msg_is_to(user, jamm->to, jamm->daddress, conf.mail_conferences[msgconf]->nettype, conf.mail_conferences[msgconf]->realnames, msgconf) &&
				    !msg_is_from(user, jamm->from, jamm->oaddress, conf.mail_conferences[msgconf]->nettype, conf.mail_conferences[msgconf]->realnames, msgconf) &&
					!msg_is_to(user, jamm->to, jamm->daddress, conf.mail_conferences[msgconf]->nettype, !conf.mail_conferences[msgconf]->realnames, msgconf) &&
				    !msg_is_from(user, jamm->from, jamm->oaddress, conf.mail_conferences[msgconf]->nettype, !conf.mail_conferences[msgconf]->realnames, msgconf)) {

					if (jamm->subject != NULL) {
						free(jamm->subject);
					}
					if (jamm->from != NULL) {
						free(jamm->from);
					}
					if (jamm->to != NULL) {
						free(jamm->to);
					}
					if (jamm->oaddress != NULL) {
						free(jamm->oaddress);
					}
					if (jamm->daddress != NULL) {
						free(jamm->daddress);
					}
					if (jamm->msgid != NULL) {
						free(jamm->msgid);
					}
					if (jamm->replyid != NULL) {
						free(jamm->replyid);
					}
					free(jamm->msg_h);
					free(jamm);
					k++;
					continue;
				}
			}

			if (msghs->msg_count == 0) {
				msghs->msgs = (struct jam_msg **)malloc(sizeof(struct jam_msg *));
			} else {
				msghs->msgs = (struct jam_msg **)realloc(msghs->msgs, sizeof(struct jam_msg *) * (msghs->msg_count + 1));
			}

			msghs->msgs[msghs->msg_count] = jamm;
			msghs->msg_count++;
			k++;
		}

	} else {
		JAM_CloseMB(jb);
		return NULL;
	}
	JAM_CloseMB(jb);
	return msghs;
}

char *external_editor(struct user_record *user, char *to, char *from, char *quote, int qlen, char *qfrom, char *subject, int email) {
	char c;
	FILE *fptr;
	char *body = NULL;
	char buffer[256];
	int len;
	int totlen;
	char *body2 = NULL;
	char *tagline;
	int i;
	int j;
	struct stat s;
	struct utsname name;



	if (conf.external_editor_cmd != NULL && user->exteditor != 0) {
		if (user->exteditor == 2) {
			s_printf(get_string(85));
			c = s_getc();
		} else {
			c = 'y';
		}
		if (tolower(c) == 'y') {

			sprintf(buffer, "%s/node%d", conf.bbs_path, mynode);

			if (stat(buffer, &s) != 0) {
				mkdir(buffer, 0755);
			}

			sprintf(buffer, "%s/node%d/MSGTMP", conf.bbs_path, mynode);

			if (stat(buffer, &s) == 0) {
				remove(buffer);
			}

			// write msgtemp
			if (quote != NULL) {
				fptr = fopen(buffer, "w");
				for (i=0;i<qlen;i++) {
					if (quote[i] == '\r') {
						fprintf(fptr, "\r\n");
					} else if (quote[i] == 0x1) {
						continue;
					} else if (quote[i] == '\e' && quote[i + 1] == '[') {
						while (strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", quote[i]) == NULL)
							i++;
					} else if (quote[i] != '\n') {
						fprintf(fptr, "%c", quote[i]);
					}
				}
				fclose(fptr);
			}
			sprintf(buffer, "%s/node%d/MSGINF", conf.bbs_path, mynode);
			fptr = fopen(buffer, "w");
			fprintf(fptr, "%s\r\n", user->loginname);
			if (qfrom != NULL) {
				fprintf(fptr, "%s\r\n", qfrom);
			} else {
				fprintf(fptr, "%s\r\n", to);
			}
			fprintf(fptr, "%s\r\n", subject);
			fprintf(fptr, "0\r\n");
			if (email == 1) {
				fprintf(fptr, "E-Mail\r\n");
				fprintf(fptr, "YES\r\n");
			} else {
				fprintf(fptr, "%s\r\n", conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->name);
				if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NETMAIL_AREA){
					fprintf(fptr, "YES\r\n");
				} else {
					fprintf(fptr, "NO\r\n");
				}
			}
			fclose(fptr);

			rundoor(user, conf.external_editor_cmd, conf.external_editor_stdio, conf.external_editor_codepage);

			// readin msgtmp
			sprintf(buffer, "%s/node%d/MSGTMP", conf.bbs_path, mynode);
			fptr = fopen(buffer, "r");
			if (!fptr) {
				return NULL;
			}

			totlen = 0;
			len = fread(buffer, 1, 256, fptr);
			while (len > 0) {
				totlen += len;
				if (body == NULL) {
					body = (char *)malloc(totlen + 1);
				} else {
					body = (char *)realloc(body, totlen + 1);
				}



				memcpy(&body[totlen - len], buffer, len);
				body[totlen] = '\0';

				len = fread(buffer, 1, 256, fptr);
			}

			fclose(fptr);

			if (email == 1) {
				tagline = conf.default_tagline;
			} else {
				if (conf.mail_conferences[user->cur_mail_conf]->tagline != NULL) {
					tagline = conf.mail_conferences[user->cur_mail_conf]->tagline;
				} else {
					tagline = conf.default_tagline;
				}
			}

			uname(&name);


			if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_FIDO && !email) {
				if (conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point == 0) {
					snprintf(buffer, 256, "\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s (%d:%d/%d)\r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline, conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
																																						  conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
																																						  conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node);
				} else {
					snprintf(buffer, 256, "\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s (%d:%d/%d.%d)\r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline, conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
																																						  conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
																																						  conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
																																						  conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point);
				}
			} else {
				snprintf(buffer, 256, "\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s \r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline);
			}
			body2 = (char *)malloc(totlen + 2 + strlen(buffer));

			j = 0;

			for (i=0;i<totlen;i++) {
				if (body[i] == '\n') {
					continue;
				} else if (body[i] == '\0') {
					continue;
				}
				body2[j++] = body[i];
				body2[j] = '\0';
			}


			strcat(body2, buffer);

			free(body);

			return body2;
		}
	}
	return editor(user, quote, qlen, qfrom, email);
}

char *editor(struct user_record *user, char *quote, int quotelen, char *from, int email) {
	int lines = 0;
	char buffer[256];
	char linebuffer[80];
	int doquit = 0;
	char **content = NULL;
	int i;
	char *msg;
	int size = 0;
	int quotelines = 0;
	char **quotecontent;
	int lineat=0;
	int qfrom,qto;
	int z;
	char *tagline;
	struct utsname name;
	char next_line_buffer[80];

	memset(next_line_buffer, 0, 80);

	if (quote != NULL) {
		for (i=0;i<quotelen;i++) {
			if (quote[i] == '\r' || lineat == 67) {
				if (quotelines == 0) {
					quotecontent = (char **)malloc(sizeof(char *));
				} else {
					quotecontent = (char **)realloc(quotecontent, sizeof(char *) * (quotelines + 1));
				}

				quotecontent[quotelines] = (char *)malloc(strlen(linebuffer) + 4);
				sprintf(quotecontent[quotelines], "%c> %s", from[0], linebuffer);
				quotelines++;
				lineat = 0;
				linebuffer[0] = '\0';
				if (quote[i] != '\r') {
					i--;
				}
			} else {
				if (quote[i] == 27) {
					while (strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", quote[i]) == NULL)
							i++;
					continue;
				}
				
				linebuffer[lineat++] = quote[i];
				linebuffer[lineat] = '\0';
			}
		}
	}
	s_printf(get_string(86));
	s_printf(get_string(87));

	while(!doquit) {
		s_printf(get_string(88), lines, next_line_buffer);
		strcpy(linebuffer, next_line_buffer);
		s_readstring(&linebuffer[strlen(next_line_buffer)], 70 - strlen(next_line_buffer));
		memset(next_line_buffer, 0, 70);

		if (strlen(linebuffer) == 70 && linebuffer[69] != ' ') {
			for (i=strlen(linebuffer) - 1;i > 15;i--) {
				if (linebuffer[i] == ' ') {
					linebuffer[i] = '\0';
					strcpy(next_line_buffer, &linebuffer[i+1]);
					s_printf("\e[%dD\e[0K", 70 - i);
					break;
				}
			}
		}

		if (linebuffer[0] == '/' && strlen(linebuffer) == 2) {
			if (toupper(linebuffer[1]) == 'S') {
				for (i=0;i<lines;i++) {
					size += strlen(content[i]) + 1;
				}
				size ++;

				if (conf.mail_conferences[user->cur_mail_conf]->tagline != NULL) {
					tagline = conf.mail_conferences[user->cur_mail_conf]->tagline;
				} else {
					tagline = conf.default_tagline;
				}
				uname(&name);
				if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_FIDO && !email) {
					if (conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point == 0) {
						snprintf(buffer, 256, "\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s (%d:%d/%d)\r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline, conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
																																							  conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
																																							  conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node);
					} else {
						snprintf(buffer, 256, "\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s (%d:%d/%d.%d)\r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline, conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
																																							  conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
																																							  conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
																																							  conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point);
					}
				} else {
					snprintf(buffer, 256, "\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s \r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline);
				}
				size += 2;
				size += strlen(buffer);

				msg = (char *)malloc(size);
				memset(msg, 0, size);
				for (i=0;i<lines;i++) {
					strcat(msg, content[i]);
					strcat(msg, "\r");
					free(content[i]);
				}

				strcat(msg, buffer);

				free(content);
				if (quote != NULL) {
					for (i=0;i<quotelines;i++) {
						free(quotecontent[i]);
					}
					free(quotecontent);
				}
				return msg;
			} else  if (toupper(linebuffer[1]) == 'A') {
				for (i=0;i<lines;i++) {
					free(content[i]);
				}
				if (lines > 0) {
					free(content);
				}

				if (quote != NULL) {
					for (i=0;i<quotelines;i++) {
						free(quotecontent[i]);
					}
					free(quotecontent);
				}

				return NULL;
			} else if (toupper(linebuffer[1]) == 'Q') {
				if (quote == NULL) {
					s_printf(get_string(89));
				} else {
					s_printf("\r\n");
					for (i=0;i<quotelines;i++) {
						s_printf(get_string(88), i, quotecontent[i]);
					}

					s_printf(get_string(90));
					s_readstring(buffer, 5);
					qfrom = atoi(buffer);
					s_printf(get_string(91));
					s_readstring(buffer, 5);
					qto = atoi(buffer);
					s_printf("\r\n");

					if (qto > quotelines) {
						qto = quotelines;
					}
					if (qfrom < 0) {
						qfrom = 0;
					}
					if (qfrom > qto) {
						s_printf(get_string(92));
					}

					for (i=qfrom;i<=qto;i++) {
						if (lines == 0) {
							content = (char **)malloc(sizeof(char *));
						} else {
							content = (char **)realloc(content, sizeof(char *) * (lines + 1));
						}

						content[lines] = strdup(quotecontent[i]);
						lines++;
					}

					s_printf(get_string(86));
					s_printf(get_string(87));

					for (i=0;i<lines;i++) {
						s_printf(get_string(88), i, content[i]);
					}
				}
			} else if (toupper(linebuffer[1]) == 'L') {
				s_printf(get_string(86));
				s_printf(get_string(87));

				for (i=0;i<lines;i++) {
					s_printf(get_string(88), i, content[i]);
				}
			} else if (linebuffer[1] == '?') {
				s_printf(get_string(93));
				s_printf(get_string(94));
				s_printf(get_string(95));
				s_printf(get_string(96));
				s_printf(get_string(97));
				s_printf(get_string(98));
				s_printf(get_string(99));
				s_printf(get_string(100));
			} else if (toupper(linebuffer[1]) == 'D') {
				s_printf(get_string(101));
				s_readstring(buffer, 6);
				if (strlen(buffer) == 0) {
					s_printf(get_string(39));
				} else {
					z = atoi(buffer);
					if (z < 0 || z >= lines) {
						s_printf(get_string(39));
					} else {
						for (i=z;i<lines-1;i++) {
							free(content[i]);
							content[i] = strdup(content[i+1]);
						}
						free(content[i]);
						lines--;
						content = (char **)realloc(content, sizeof(char *) * lines);
					}
				}
			} else if (toupper(linebuffer[1]) == 'E') {
				s_printf(get_string(102));
				s_readstring(buffer, 6);
				if (strlen(buffer) == 0) {
					s_printf(get_string(39));
				} else {
					z = atoi(buffer);
					if (z < 0 || z >= lines) {
						s_printf(get_string(39));
					} else {
						s_printf(get_string(88), z, content[z]);
						s_printf(get_string(103), z);
						s_readstring(linebuffer, 70);
						free(content[z]);
						content[z] = strdup(linebuffer);
					}
				}
			} else if (toupper(linebuffer[1]) == 'I') {
				s_printf(get_string(104));
				s_readstring(buffer, 6);
				if (strlen(buffer) == 0) {
					s_printf(get_string(39));
				} else {
					z = atoi(buffer);
					if (z < 0 || z >= lines) {
						s_printf(get_string(39));
					} else {
						s_printf(get_string(103), z);
						s_readstring(linebuffer, 70);
						lines++;
						content = (char **)realloc(content, sizeof(char *) * lines);

						for (i=lines;i>z;i--) {
							content[i] = content[i-1];
						}

						content[z] = strdup(linebuffer);
					}
				}
			}
		} else {
			if (lines == 0) {
				content = (char **)malloc(sizeof(char *));
			} else {
				content = (char **)realloc(content, sizeof(char *) * (lines + 1));
			}

			content[lines] = strdup(linebuffer);

			lines++;
		}
	}
	if (quote != NULL) {
		for (i=0;i<quotelines;i++) {
			free(quotecontent[i]);
		}
		free(quotecontent);
	}
	return NULL;
}

struct character_t {
	char c;
	int fg;
	int bg;
};

void unmangle_ansi(char *body, int len, char **body_out, int *body_len) {
	// count lines
	int line_count = 1;
	int line_at = 1;
	int char_at = 1;
	int fg = 0x07;
	int bg = 0x00;
	int state = 0;
	int save_char_at = 0;
	int save_line_at = 0;
	int params[16];
	int param_count = 0;
	int bold = 0;
	char *out;
	int out_len;
	int out_max;
	char buffer[1024];
	int buf_at;
	int i, j, k;
	struct character_t ***fake_screen;
	int ansi;
	
	
	line_at = 1;
	char_at = 1;
	
	for (i=0;i<len;i++) {

		if (state == 0) {
			if (body[i] == 27) {
				state = 1;
				continue;
			} else {
				if (body[i] == '\r') {
					char_at = 1;
					line_at++;
				} else {
					char_at++;
					while (char_at > 80) {
						line_at++;
						char_at -= 80;
					}
				}
				
				if (line_at > line_count) {
					line_count = line_at;
				}
			}
		} else if (state == 1) {
			if (body[i] == '[') {
				state = 2;
				continue;
			}
		} else if (state == 2) {
			param_count = 0;
			for (j=0;j<16;j++) {
				params[j] = 0;
			}
			state = 3;
		}
		if (state == 3) {
			if (body[i] == ';') {
				if (param_count < 15) {
					param_count++;
				}
				continue;
			} else if (body[i] >= '0' && body[i] <= '9') {
				if (!param_count) param_count = 1;
				params[param_count-1] = params[param_count-1] * 10 + (body[i] - '0');
				continue;
			} else {
				state = 4;
			}
		}
		
		if (state == 4) {
			switch(body[i]) {
				case 'H':
				case 'f':
					if (params[0]) params[0]--;
					if (params[1]) params[1]--;
					line_at = params[0] + 1;
					char_at = params[1] + 1;
					
					if (char_at > 80) {
						char_at = 80;
					}
					
					if (line_at > line_count) {
						line_count = line_at;
					}
					
					state = 0;
					break;
				case 'A':
					if (param_count > 0) {
						line_at = line_at - params[0];
					} else {
						line_at--;
					}
					if (line_at < 1) {
						line_at = 1;
					}
					state = 0;
					break;
				case 'B':
					if (param_count > 0) {
						line_at = line_at + params[0];
					} else {
						line_at++;
					}
					if (line_at > line_count) {
						line_count = line_at;
					}
					state = 0;
					break;
				case 'C':
					if (param_count > 0) {
						char_at = char_at + params[0];
					} else {
						char_at ++;
					}
					if (char_at > 80) {
						char_at = 80;
					}
					state = 0;
					break;
				case 'D':
					if (param_count > 0) {
						char_at = char_at - params[0];
					} else {
						char_at --;
					}
					if (char_at < 1) {
						char_at = 1;
					}
					state = 0;
					break;
				case 's':
					save_char_at = char_at;
					save_line_at = line_at;
					state = 0;
					break;
				case 'u':
					char_at = save_char_at;
					line_at = save_line_at;
					state = 0;
					break;
				default:
					state = 0;
					break;
			}
		}
	}	
	
	fake_screen = (struct character_t ***)malloc(sizeof(struct character_t **) * line_count);
	for (i=0;i<line_count;i++) {
		fake_screen[i] = (struct character_t **)malloc(sizeof(struct character_t*) * 80);
		for (j=0;j<80;j++) {
			fake_screen[i][j] = (struct character_t *)malloc(sizeof(struct character_t));
			fake_screen[i][j]->c = ' ';
			fake_screen[i][j]->fg = fg;
			fake_screen[i][j]->bg = bg;
		}
	}
	
	line_at = 1;
	char_at = 1;
	
	for (i=0;i<len;i++) {

		if (state == 0) {
			if (body[i] == 27) {
				state = 1;
				continue;
			} else {
				if (body[i] == '\r') {
					char_at = 1;
					line_at++;
				} else {
					if (line_at > line_count) line_at = line_count;
					fake_screen[line_at -1][char_at - 1]->c = body[i];
					fake_screen[line_at -1][char_at - 1]->fg = fg;
					fake_screen[line_at -1][char_at - 1]->bg = bg;
					char_at++;
					while (char_at > 80) {
						line_at++;
						char_at -= 80;
					}
				}
			}
		} else if (state == 1) {
			if (body[i] == '[') {
				state = 2;
				continue;
			}
		} else if (state == 2) {
			param_count = 0;
			for (j=0;j<16;j++) {
				params[j] = 0;
			}
			state = 3;
		}
		if (state == 3) {
			if (body[i] == ';') {
				if (param_count < 15) {
					param_count++;
				}
				continue;
			} else if (body[i] >= '0' && body[i] <= '9') {
				if (!param_count) param_count = 1;
				params[param_count-1] = params[param_count-1] * 10 + (body[i] - '0');
				continue;
			} else {
				state = 4;
			}
		}
		
		if (state == 4) {
			switch(body[i]) {
				case 'H':
				case 'f':
					if (params[0]) params[0]--;
					if (params[1]) params[1]--;
					line_at = params[0] + 1;
					char_at = params[1] + 1;
					state = 0;
					break;
				case 'A':
					if (param_count > 0) {
						line_at = line_at - params[0];
					} else {
						line_at--;
					}
					if (line_at < 1) {
						line_at = 1;
					}
					state = 0;
					break;
				case 'B':
					if (param_count > 0) {
						line_at = line_at + params[0];
					} else {
						line_at++;
					}
					if (line_at > line_count) {
						line_at = line_count;
					}
					state = 0;
					break;
				case 'C':
					if (param_count > 0) {
						char_at = char_at + params[0];
					} else {
						char_at ++;
					}
					if (char_at > 80) {
						char_at = 80;
					}
					state = 0;
					break;
				case 'D':
					if (param_count > 0) {
						char_at = char_at - params[0];
					} else {
						char_at --;
					}
					if (char_at < 1) {
						char_at = 1;
					}
					state = 0;
					break;
				case 's':
					save_char_at = char_at;
					save_line_at = line_at;
					state = 0;
					break;
				case 'u':
					char_at = save_char_at;
					line_at = save_line_at;
					state = 0;
					break;
				case 'm':
					for (j=0;j<param_count;j++) {
						switch(params[j]) {
							case 0:
								fg = 0x07;
								bg = 0x00;
								bold = 0;
								break;
							case 1:
								bold = 1;
								if (fg < 0x08) {
									fg += 0x08;
								}
								break;
							case 2:
								bold = 0;
								if (fg > 0x07) {
									fg -= 0x08;
								}
								break;
							case 30:
								if (bold) {
									fg = 0x08;
								} else {
									fg = 0x00;
								}
								break;
							case 31:
								if (bold) {
									fg = 0x0C;
								} else {
									fg = 0x04;
								}
								break;
							case 32:
								if (bold) {
									fg = 0x0A;
								} else {
									fg = 0x02;
								}
								break;
							case 33:
								if (bold) {
									fg = 0x0E;
								} else {
									fg = 0x06;
								}
								break;
							case 34:
								if (bold) {
									fg = 0x09;
								} else {
									fg = 0x01;
								}
								break;
							case 35:
								if (bold) {
									fg = 0x0D;
								} else {
									fg = 0x05;
								}
								break;
							case 36:
								if (bold) {
									fg = 0x0B;
								} else {
									fg = 0x03;
								}
								break;
							case 37:
								if (bold) {
									fg = 0x0F;
								} else {
									fg = 0x07;
								}
								break;
							case 40:
								bg = 0x00;
								break;
							case 41:
								bg = 0x04;
								break;
							case 42:
								bg = 0x02;
								break;
							case 43:
								bg = 0x06;
								break;
							case 44:
								bg = 0x01;
								break;
							case 45:
								bg = 0x05;
								break;
							case 46:
								bg = 0x03;
								break;
							case 47:
								bg = 0x07;
								break;
							}
								
					}
					state = 0;
					break;
				case 'K':
					if (params[0] == 0) {
						for (k=char_at-1;k<80;k++) {
							fake_screen[line_at-1][k]->c = ' ';
							fake_screen[line_at-1][k]->fg = fg;
							fake_screen[line_at-1][k]->bg = bg;
						}
					} else if (params[0] == 1) {
						for (k=0;k<char_at;k++) {
							fake_screen[line_at-1][k]->c = ' ';
							fake_screen[line_at-1][k]->fg = fg;
							fake_screen[line_at-1][k]->bg = bg;
						}
					} else if (params[0] == 2) {
						for (k=0;k<80;k++) {
							fake_screen[line_at-1][k]->c = ' ';
							fake_screen[line_at-1][k]->fg = fg;
							fake_screen[line_at-1][k]->bg = bg;
						}
					}
					state = 0;
					break;
				case 'J':
					if (params[0] == 0) {
						for (k=char_at-1;k<80;k++) {
							fake_screen[line_at-1][k]->c = ' ';
							fake_screen[line_at-1][k]->fg = fg;
							fake_screen[line_at-1][k]->bg = bg;
						}
						
						for (k=line_at;k<line_count;k++) {
							for (j=0;j<80;j++) {
								fake_screen[k][j]->c = ' ';
								fake_screen[k][j]->fg = fg;
								fake_screen[k][j]->bg = bg;
							}
						}
					} else if (params[0] == 1) {
						for (k=0;k<char_at;k++) {
							fake_screen[line_at-1][k]->c = ' ';
							fake_screen[line_at-1][k]->fg = fg;
							fake_screen[line_at-1][k]->bg = bg;
						}
						
						for (k=line_at-2;k>=0;k--) {
							for (j=0;j<80;j++) {
								fake_screen[k][j]->c = ' ';
								fake_screen[k][j]->fg = fg;
								fake_screen[k][j]->bg = bg;
							}
						}
					} else if (params[0] == 2) {
						for (k=0;k<line_count;k++) {
							for (j=0;j<80;j++) {
								fake_screen[k][j]->c = ' ';
								fake_screen[k][j]->fg = fg;
								fake_screen[k][j]->bg = bg;
							}
						}
					}
					state = 0;
					break;
				default:
					// bad ansi
					state = 0;
					break;
			}
		}
	}

	fg = 0x07;
	bg = 0x00;
	
	out_max = 256;
	out_len = 0;
	out = (char *)malloc(256);
	
	for (i=0;i<line_count;i++) {
		buf_at = 0;
		for (j=0;j<79;j++) {
			if (fake_screen[i][j]->fg != fg || fake_screen[i][j]->bg != bg) {
				buffer[buf_at++] = 27;
				buffer[buf_at++] = '[';
				fg = fake_screen[i][j]->fg;
				if (fg < 0x08) {
					buffer[buf_at++] = '0';
					buffer[buf_at++] = ';';
					buffer[buf_at++] = '3';
					switch (fg) {
						case 0x00:
							buffer[buf_at++] = '0';
							break;
						case 0x04:
							buffer[buf_at++] = '1';
							break;
						case 0x02:
							buffer[buf_at++] = '2';
							break;
						case 0x06:
							buffer[buf_at++] = '3';
							break;
						case 0x01:
							buffer[buf_at++] = '4';
							break;
						case 0x05:
							buffer[buf_at++] = '5';
							break;
						case 0x03:
							buffer[buf_at++] = '6';
							break;
						case 0x07:
							buffer[buf_at++] = '7';
							break;
							
						
					}
				} else {
					buffer[buf_at++] = '1';
					buffer[buf_at++] = ';';
					buffer[buf_at++] = '3';
					switch (fg) {
						case 0x08:
							buffer[buf_at++] = '0';
							break;
						case 0x0C:
							buffer[buf_at++] = '1';
							break;
						case 0x0A:
							buffer[buf_at++] = '2';
							break;
						case 0x0E:
							buffer[buf_at++] = '3';
							break;
						case 0x09:
							buffer[buf_at++] = '4';
							break;
						case 0x0D:
							buffer[buf_at++] = '5';
							break;
						case 0x0B:
							buffer[buf_at++] = '6';
							break;
						case 0x0F:
							buffer[buf_at++] = '7';
							break;
								
							
					}	
				}
			
			
				bg = fake_screen[i][j]->bg;
				buffer[buf_at++] = ';';
				buffer[buf_at++] = '4';
				switch (bg) {
					case 0x00:
						buffer[buf_at++] = '0';
						break;
					case 0x04:
						buffer[buf_at++] = '1';
						break;
					case 0x02:
						buffer[buf_at++] = '2';
						break;
					case 0x06:
						buffer[buf_at++] = '3';
						break;
					case 0x01:
						buffer[buf_at++] = '4';
						break;
					case 0x05:
						buffer[buf_at++] = '5';
						break;
					case 0x03:
						buffer[buf_at++] = '6';
						break;
					case 0x07:
						buffer[buf_at++] = '7';
						break;
							
							
				}					
				buffer[buf_at++] = 'm';
			}
			buffer[buf_at++] = fake_screen[i][j]->c;
		}
	
		
		while (buf_at > 0 && buffer[buf_at-1] == ' ') {
			buf_at--;
		}
		
		buffer[buf_at++] = '\r';

		while (buf_at + out_len > out_max) {
			out_max += 256;
			out = (char *)realloc(out, out_max);
		}
		
		memcpy(&out[out_len], buffer, buf_at);
		out_len += buf_at;

	}
	
	for (i=0;i<line_count;i++) {
		for (j=0;j<80;j++) {
			free(fake_screen[i][j]);
		}
		free(fake_screen[i]);
	}
	free(fake_screen);
	
	while (out[out_len-2] == '\r') {
		out_len--;
	}
	
	
	
	*body_out = out;
	*body_len = out_len;
}
	


void read_message(struct user_record *user, struct msg_headers *msghs, int mailno) {
	s_JamBase *jb;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;
	s_JamSubfield jsf;
	s_JamLastRead jlr;


	char buffer[256];
	int z, z2;
	struct tm msg_date;

	char *subject = NULL;
	char *from = NULL;
	char *to = NULL;
	char *body = NULL;
	char *body2 = NULL;
	int lines = 0;
	char c;
	char *replybody;
	struct fido_addr *from_addr = NULL;
	int i, j;
	int doquit = 0;
	int skip_line = 0;
	int chars = 0;
	int ansi;
	int sem_fd;
    char **msg_lines;
    int msg_line_count;
    int start_line;
    int should_break;
    int position;
    int y;
    
	jb = open_jam_base(conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
	if (!jb) {
		dolog("Error opening JAM base.. %s", conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
		return;
	}

	while (!doquit) {

		if (JAM_ReadLastRead(jb, user->id, &jlr) == JAM_NO_USER) {
			jlr.UserCRC = JAM_Crc32(user->loginname, strlen(user->loginname));
			jlr.UserID = user->id;
			jlr.HighReadMsg = msghs->msgs[mailno]->msg_h->MsgNum;
		}

		jlr.LastReadMsg = msghs->msgs[mailno]->msg_h->MsgNum;
		if (jlr.HighReadMsg < msghs->msgs[mailno]->msg_h->MsgNum) {
			jlr.HighReadMsg = msghs->msgs[mailno]->msg_h->MsgNum;
		}

		if (msghs->msgs[mailno]->oaddress != NULL && conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_FIDO) {
			from_addr = parse_fido_addr(msghs->msgs[mailno]->oaddress);
			s_printf(get_string(105), msghs->msgs[mailno]->from, from_addr->zone, from_addr->net, from_addr->node, from_addr->point);
			free(from_addr);
		} else {
			s_printf(get_string(106), msghs->msgs[mailno]->from);
		}
		s_printf(get_string(107), msghs->msgs[mailno]->to, conf.mail_conferences[user->cur_mail_conf]->name);
		s_printf(get_string(108), msghs->msgs[mailno]->subject, conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->name);
		gmtime_r((time_t *)&msghs->msgs[mailno]->msg_h->DateWritten, &msg_date);
		sprintf(buffer, "%s", asctime(&msg_date));
		buffer[strlen(buffer) - 1] = '\0';
		s_printf(get_string(109), buffer, mailno + 1, msghs->msg_count);
		s_printf(get_string(110), (msghs->msgs[mailno]->msg_h->Attribute & JAM_MSG_SENT ? "SENT" : ""));
		s_printf(get_string(111));

		body = (char *)malloc(msghs->msgs[mailno]->msg_h->TxtLen);

		JAM_ReadMsgText(jb, msghs->msgs[mailno]->msg_h->TxtOffset, msghs->msgs[mailno]->msg_h->TxtLen, (char *)body);
		JAM_WriteLastRead(jb, user->id, &jlr);

		z2 = msghs->msgs[mailno]->msg_h->TxtLen;

		lines = 0;
		chars = 0;
        
        body2 = body;
        z = z2;
        
        unmangle_ansi(body2, z, &body, &z2);
        free(body2);
        msg_line_count = 0;
        start_line = 0;
        
        // count the number of lines...
        for (z=0;z<z2;z++) {
            if (body[z] == '\r' || chars == 80) {
                if (msg_line_count == 0) {
                    msg_lines = (char **)malloc(sizeof(char *));
                } else {
                    msg_lines = (char **)realloc(msg_lines, sizeof(char *) * (msg_line_count + 1));
                }
                
                msg_lines[msg_line_count] = (char *)malloc(sizeof(char) * (z - start_line + 1));
                
                if (z == start_line) {
                    msg_lines[msg_line_count][0] = '\0';
                } else {
                    strncpy(msg_lines[msg_line_count], &body[start_line], z - start_line);
                    msg_lines[msg_line_count][z-start_line] = '\0';
                }
                msg_line_count++;
                if (body[z] == '\r') {
                    start_line = z + 1;
				} else {
                    start_line = z;
                }
                chars = 0;
            } else {
                if (body[z] == 27) {
                    ansi = z;
                    while (strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", body[z]) == NULL)
                        z++;
                    if (body[z] == 'm') {
                        // do nothing
                    } else {
						y = ansi;
                        for (j=z+1;j<z2;j++) {
                            body[y] = body[j];
                            y++;
                        }
                        z2 = z2 - (z2 - y);
                        z = ansi - 1;
                    }
                } else {
                    chars ++;
                }
            }
        }
        
        lines = 0;
        
        position = 0;
        should_break = 0;
       
        while (!should_break) {
            s_printf("\e[7;1H\e[J");
            for (z=position;z<msg_line_count;z++) {
                
                s_printf("%s\e[K\r\n", msg_lines[z]);
                
                if (z - position >= 15) {
                    break;
                }
            }
            s_printf(get_string(187));
            s_printf(get_string(186));
            c = s_getc();
            
            if (tolower(c) == 'r') {
                should_break = 1;
            } else if (tolower(c) == 'q') {
                should_break = 1;
            } else if (c == '\e') {
                c = s_getc();
                if (c == 91) {
                    c = s_getc();
                    if (c == 65) {
                        position--;
                        if (position < 0) {
                            position = 0;
                        }
                    } else if (c == 66) {
                        position++;
                        if (position + 15 > msg_line_count) {
                            position--;
                        }
                    } else if (c == 67) {
                        c = ' ';
                        should_break = 1;
                    } else if (c == 68) {
                        c = 'b';
                        should_break = 1;
                    }
                }
            }
            
        }

		if (tolower(c) == 'r') {
			JAM_CloseMB(jb);
			if (user->sec_level < conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->write_sec_level) {
				s_printf(get_string(113));
			} else {
				if (msghs->msgs[mailno]->subject != NULL) {
					if (strncasecmp(msghs->msgs[mailno]->subject, "RE:", 3) != 0) {
						snprintf(buffer, 256, "RE: %s", msghs->msgs[mailno]->subject);
					} else {
						snprintf(buffer, 256, "%s", msghs->msgs[mailno]->subject);
					}
				}
				subject = (char *)malloc(strlen(buffer) + 1);
				strcpy(subject, buffer);

				s_printf(get_string(114), subject);
				s_printf(get_string(115));

				c = s_getc();

				if (tolower(c) == 'y') {
					s_printf(get_string(116));
					s_readstring(buffer, 25);

					if (strlen(buffer) == 0) {
						s_printf(get_string(117));
					} else {
						free(subject);
						subject = (char *)malloc(strlen(buffer) + 1);
						strcpy(subject, buffer);
					}
				}
				s_printf("\r\n");

				if (msghs->msgs[mailno]->from != NULL) {
					strcpy(buffer, msghs->msgs[mailno]->from);
				}
				if (conf.mail_conferences[user->cur_mail_conf]->realnames == 0) {
					from = (char *)malloc(strlen(user->loginname) + 1);
					strcpy(from, user->loginname);
				} else {
					from = (char *)malloc(strlen(user->firstname) + strlen(user->lastname) + 2);
					sprintf(from, "%s %s", user->firstname, user->lastname);
				}
				if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NEWSGROUP_AREA) {
					to = (char *)malloc(4);
					strcpy(to, "ALL");
				} else {
					to = (char *)malloc(strlen(buffer) + 1);
					strcpy(to, buffer);
				}
				replybody = external_editor(user, to, from, body, z2, msghs->msgs[mailno]->from, subject, 0);
				if (replybody != NULL) {

					jb = open_jam_base(conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
					if (!jb) {
						dolog("Error opening JAM base.. %s", conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
						free(replybody);
						free(body);
						free(subject);
						free(to);
						free(from);
                        for (i=0;i<msg_line_count;i++) {
                            free(msg_lines[i]);
                        }
                        free(msg_lines);                        
						return;
					}

					JAM_ClearMsgHeader( &jmh );
					jmh.DateWritten = utc_to_local(time(NULL));
					jmh.Attribute |= JAM_MSG_LOCAL;

					jsp = JAM_NewSubPacket();
					jsf.LoID   = JAMSFLD_SENDERNAME;
					jsf.HiID   = 0;
					jsf.DatLen = strlen(from);
					jsf.Buffer = (char *)from;
					JAM_PutSubfield(jsp, &jsf);

					jsf.LoID   = JAMSFLD_RECVRNAME;
					jsf.HiID   = 0;
					jsf.DatLen = strlen(to);
					jsf.Buffer = (char *)to;
					JAM_PutSubfield(jsp, &jsf);

					jsf.LoID   = JAMSFLD_SUBJECT;
					jsf.HiID   = 0;
					jsf.DatLen = strlen(subject);
					jsf.Buffer = (char *)subject;
					JAM_PutSubfield(jsp, &jsf);



					if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_ECHOMAIL_AREA || conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NEWSGROUP_AREA) {
						jmh.Attribute |= JAM_MSG_TYPEECHO;

						if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_FIDO) {
							if (conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point) {
								sprintf(buffer, "%d:%d/%d.%d", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point);
							} else {
								sprintf(buffer, "%d:%d/%d", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node);
							}
							jsf.LoID   = JAMSFLD_OADDRESS;
							jsf.HiID   = 0;
							jsf.DatLen = strlen(buffer);
							jsf.Buffer = (char *)buffer;
							JAM_PutSubfield(jsp, &jsf);


							sprintf(buffer, "%d:%d/%d.%d %08lx", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point,
															generate_msgid());

							jsf.LoID   = JAMSFLD_MSGID;
							jsf.HiID   = 0;
							jsf.DatLen = strlen(buffer);
							jsf.Buffer = (char *)buffer;
							JAM_PutSubfield(jsp, &jsf);

							if (msghs->msgs[mailno]->msgid != NULL) {
								sprintf(buffer, "%d:%d/%d.%d %s", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
										conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
										conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
										conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point,
										msghs->msgs[mailno]->msgid);
							}

							jsf.LoID   = JAMSFLD_REPLYID;
							jsf.HiID   = 0;
							jsf.DatLen = strlen(buffer);
							jsf.Buffer = (char *)buffer;
							JAM_PutSubfield(jsp, &jsf);
							jmh.ReplyCRC = JAM_Crc32(buffer, strlen(buffer));
						}
					} else if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NETMAIL_AREA) {
						jmh.Attribute |= JAM_MSG_TYPENET;
						jmh.Attribute |= JAM_MSG_PRIVATE;

						if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_FIDO) {
							if (conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point) {
								sprintf(buffer, "%d:%d/%d.%d", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point);
							} else {
								sprintf(buffer, "%d:%d/%d", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node);
							}
							jsf.LoID   = JAMSFLD_OADDRESS;
							jsf.HiID   = 0;
							jsf.DatLen = strlen(buffer);
							jsf.Buffer = (char *)buffer;
							JAM_PutSubfield(jsp, &jsf);
							jmh.MsgIdCRC = JAM_Crc32(buffer, strlen(buffer));
							from_addr = parse_fido_addr(msghs->msgs[mailno]->oaddress);
							if (from_addr != NULL) {
								if (from_addr->point) {
									sprintf(buffer, "%d:%d/%d.%d", from_addr->zone,
																from_addr->net,
																from_addr->node,
																from_addr->point);
								} else {
									sprintf(buffer, "%d:%d/%d", from_addr->zone,
																from_addr->net,
																from_addr->node);
								}
								jsf.LoID   = JAMSFLD_DADDRESS;
								jsf.HiID   = 0;
								jsf.DatLen = strlen(buffer);
								jsf.Buffer = (char *)buffer;
								JAM_PutSubfield(jsp, &jsf);
								free(from_addr);
							}


							sprintf(buffer, "%d:%d/%d.%d %08lx", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point,
															generate_msgid());

							jsf.LoID   = JAMSFLD_MSGID;
							jsf.HiID   = 0;
							jsf.DatLen = strlen(buffer);
							jsf.Buffer = (char *)buffer;
							JAM_PutSubfield(jsp, &jsf);
							jmh.MsgIdCRC = JAM_Crc32(buffer, strlen(buffer));
							if (msghs->msgs[mailno]->msgid != NULL) {
								sprintf(buffer, "%d:%d/%d.%d %s", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
										conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
										conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
										conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point,
										msghs->msgs[mailno]->msgid);
							}

							jsf.LoID   = JAMSFLD_REPLYID;
							jsf.HiID   = 0;
							jsf.DatLen = strlen(buffer);
							jsf.Buffer = (char *)buffer;
							JAM_PutSubfield(jsp, &jsf);
							jmh.ReplyCRC = JAM_Crc32(buffer, strlen(buffer));
						} 
					}

					while (1) {
						z = JAM_LockMB(jb, 100);
						if (z == 0) {
							break;
						} else if (z == JAM_LOCK_FAILED) {
							sleep(1);
						} else {
							free(replybody);
							free(body);
							free(subject);
							free(to);
							free(from);
							dolog("Failed to lock msg base!");
                            for (i=0;i<msg_line_count;i++) {
                                free(msg_lines[i]);
                            }
                            free(msg_lines);                            
							return;
						}
					}
					if (JAM_AddMessage(jb, &jmh, jsp, (char *)replybody, strlen(replybody))) {
						dolog("Failed to add message");
					} else {
						if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NETMAIL_AREA) {
							if (conf.netmail_sem != NULL) {
								sem_fd = open(conf.netmail_sem, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
								close(sem_fd);
							}
						} else if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_ECHOMAIL_AREA || conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NEWSGROUP_AREA) {
							if (conf.echomail_sem != NULL) {
								sem_fd = open(conf.echomail_sem, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
								close(sem_fd);
							}
						}
					}

					JAM_UnlockMB(jb);

					JAM_DelSubPacket(jsp);
					free(replybody);
					JAM_CloseMB(jb);
					doquit = 1;
				} else {
					jb = open_jam_base(conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
				}
			}
			free(body);

			if (from != NULL) {
				free(from);
			}

			if (to != NULL) {
				free(to);
			}

			if (subject != NULL) {
				free(subject);
			}

		} else if (tolower(c) == 'q') {
            free(body);
			doquit = 1;
		} else if (c == ' ') {
			mailno++;
			if (mailno >= msghs->msg_count) {
				s_printf(get_string(118));
				doquit = 1;
			}
			free(body);
		} else if (tolower(c) == 'b') {
			if (mailno > 0) {
				mailno--;
			}
			free(body);
		} else {
            free(body);
        }
        for (i=0;i<msg_line_count;i++) {
            free(msg_lines[i]);
        }
        free(msg_lines);	
        msg_line_count = 0;
	}
}

void read_mail(struct user_record *user) {
	struct msg_headers *msghs;
	s_JamBase *jb;
	s_JamLastRead jlr;
	int all_unread;
	int i;
	int k;
	char buffer[7];

	s_printf("\r\n");
	// list mail in message base
	msghs = read_message_headers(user->cur_mail_conf, user->cur_mail_area, user);
	if (msghs != NULL && msghs->msg_count > 0) {
		jb = open_jam_base(conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
		if (!jb) {
			dolog("Error opening JAM base.. %s", conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
			return;
		} else {
			all_unread = 0;
			if (JAM_ReadLastRead(jb, user->id, &jlr) == JAM_NO_USER) {
				jlr.LastReadMsg = 0;
				jlr.HighReadMsg = 0;
				all_unread = 1;
			}
			JAM_CloseMB(jb);
			s_printf(get_string(120), msghs->msg_count);

			s_readstring(buffer, 6);

			if (tolower(buffer[0]) == 'n') {
				if (all_unread == 0) {
					k = jlr.HighReadMsg;
					for (i=0;i<msghs->msg_count;i++) {
						if (msghs->msgs[i]->msg_h->MsgNum == k) {
							break;
						}
					}
					i += 2;
				} else {
					i = 1;
				}
			} else {
				i = atoi(buffer);
			}

			if (i > 0 && i <= msghs->msg_count) {
				read_message(user, msghs, i - 1);
			}
		}
	}
	if (msghs != NULL) {
		free_message_headers(msghs);
	}
}

void post_message(struct user_record *user) {
	char *subject;
	char *from;
	char *to;
	char *msg;
	int closed;
	struct fido_addr *from_addr = NULL;
	char buffer[256];
	char buffer2[256];
	int z;
	int sem_fd;

	s_JamBase *jb;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;
	s_JamSubfield jsf;
	s_JamLastRead jlr;

	if (user->sec_level < conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->write_sec_level) {
		s_printf(get_string(113));
		return;
	}
	if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NEWSGROUP_AREA) {
		sprintf(buffer, "ALL");
	} else {
		s_printf(get_string(54));
		s_readstring(buffer, 16);
	}
	if (strlen(buffer) == 0) {
		strcpy(buffer, "ALL");
	}

	if (conf.mail_conferences[user->cur_mail_conf]->networked == 0 && strcasecmp(buffer, "ALL") != 0) {
		if (check_user(buffer)) {
			s_printf(get_string(55));
			return;
		}
	}
	if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NETMAIL_AREA) {
		s_printf(get_string(121));
		s_readstring(buffer2, 32);
		if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_FIDO) {
			from_addr = parse_fido_addr(buffer2);
			if (!from_addr) {
				s_printf(get_string(122));
				return;
			} else {
				if (from_addr->zone == 0 && from_addr->net == 0 && from_addr->node == 0 && from_addr->point == 0) {
					free(from_addr);
					s_printf(get_string(122));
					return;
				}
				s_printf(get_string(123), from_addr->zone, from_addr->net, from_addr->node, from_addr->point);
			}
		}
	}
	to = strdup(buffer);
	s_printf(get_string(56));
	s_readstring(buffer, 25);
	if (strlen(buffer) == 0) {
		s_printf(get_string(39));
		free(to);
		if (from_addr != NULL) {
			free(from_addr);
		}
		return;
	}
	subject = strdup(buffer);

	// post a message
	if (conf.mail_conferences[user->cur_mail_conf]->realnames == 0) {
		from = strdup(user->loginname);
	} else {
		from = (char *)malloc(strlen(user->firstname) + strlen(user->lastname) + 2);
		sprintf(from, "%s %s", user->firstname, user->lastname);
	}
	msg = external_editor(user, to, from, NULL, 0, NULL, subject, 0);

	free(from);

	if (msg != NULL) {
		jb = open_jam_base(conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
		if (!jb) {
			dolog("Error opening JAM base.. %s", conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
			free(msg);
			free(to);
			free(subject);
			return;
		}

		JAM_ClearMsgHeader( &jmh );
		jmh.DateWritten = (uint32_t)utc_to_local(time(NULL));
		jmh.Attribute |= JAM_MSG_LOCAL;
		if (conf.mail_conferences[user->cur_mail_conf]->realnames == 0) {
			strcpy(buffer, user->loginname);
		} else {
			sprintf(buffer, "%s %s", user->firstname, user->lastname);
		}

		jsp = JAM_NewSubPacket();

		jsf.LoID   = JAMSFLD_SENDERNAME;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(buffer);
		jsf.Buffer = (char *)buffer;
		JAM_PutSubfield(jsp, &jsf);

		jsf.LoID   = JAMSFLD_RECVRNAME;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(to);
		jsf.Buffer = (char *)to;
		JAM_PutSubfield(jsp, &jsf);

		jsf.LoID   = JAMSFLD_SUBJECT;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(subject);
		jsf.Buffer = (char *)subject;
		JAM_PutSubfield(jsp, &jsf);

		if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_ECHOMAIL_AREA || conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NEWSGROUP_AREA) {
			jmh.Attribute |= JAM_MSG_TYPEECHO;

			if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_FIDO) {
				if (conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point) {
					sprintf(buffer, "%d:%d/%d.%d", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point);
				} else {
					sprintf(buffer, "%d:%d/%d", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node);
				}
				jsf.LoID   = JAMSFLD_OADDRESS;
				jsf.HiID   = 0;
				jsf.DatLen = strlen(buffer);
				jsf.Buffer = (char *)buffer;
				JAM_PutSubfield(jsp, &jsf);

				sprintf(buffer, "%d:%d/%d.%d %08lx", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point,
												generate_msgid());

				jsf.LoID   = JAMSFLD_MSGID;
				jsf.HiID   = 0;
				jsf.DatLen = strlen(buffer);
				jsf.Buffer = (char *)buffer;
				JAM_PutSubfield(jsp, &jsf);
				jmh.MsgIdCRC = JAM_Crc32(buffer, strlen(buffer));

			}
		} else
		if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NETMAIL_AREA) {
			jmh.Attribute |= JAM_MSG_TYPENET;
			jmh.Attribute |= JAM_MSG_PRIVATE;
			if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_FIDO) {
				if (conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point) {
					sprintf(buffer, "%d:%d/%d.%d", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point);
				} else {
					sprintf(buffer, "%d:%d/%d", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
												conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node);
				}
				jsf.LoID   = JAMSFLD_OADDRESS;
				jsf.HiID   = 0;
				jsf.DatLen = strlen(buffer);
				jsf.Buffer = (char *)buffer;
				JAM_PutSubfield(jsp, &jsf);

				if (from_addr != NULL) {
					if (from_addr->point) {
						sprintf(buffer, "%d:%d/%d.%d", from_addr->zone,
														from_addr->net,
														from_addr->node,
														from_addr->point);
					} else {
						sprintf(buffer, "%d:%d/%d", from_addr->zone,
													from_addr->net,
													from_addr->node);
					}
					jsf.LoID   = JAMSFLD_DADDRESS;
					jsf.HiID   = 0;
					jsf.DatLen = strlen(buffer);
					jsf.Buffer = (char *)buffer;
					JAM_PutSubfield(jsp, &jsf);
					free(from_addr);
					from_addr = NULL;
				}

				sprintf(buffer, "%d:%d/%d.%d %08lx", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
													conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
													conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
													conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point,
													generate_msgid());

				jsf.LoID   = JAMSFLD_MSGID;
				jsf.HiID   = 0;
				jsf.DatLen = strlen(buffer);
				jsf.Buffer = (char *)buffer;
				JAM_PutSubfield(jsp, &jsf);
				jmh.MsgIdCRC = JAM_Crc32(buffer, strlen(buffer));
			} 
		}

		while (1) {
			z = JAM_LockMB(jb, 100);
			if (z == 0) {
				break;
			} else if (z == JAM_LOCK_FAILED) {
				sleep(1);
			} else {
				free(msg);
				free(to);
				free(subject);
				dolog("Failed to lock msg base!");
				break;
			}
		}
		if (z != 0) {
			JAM_CloseMB(jb);
			return;
		}

		if (JAM_AddMessage(jb, &jmh, jsp, (char *)msg, strlen(msg))) {
			dolog("Failed to add message");
		} else {
			if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NETMAIL_AREA) {
				if (conf.netmail_sem != NULL) {
					sem_fd = open(conf.netmail_sem, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
					close(sem_fd);
				}
			} else if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_ECHOMAIL_AREA || conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NEWSGROUP_AREA) {
				if (conf.echomail_sem != NULL) {
					sem_fd = open(conf.echomail_sem, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
					close(sem_fd);
				}
			}
		}

		JAM_UnlockMB(jb);

		JAM_DelSubPacket(jsp);
		free(msg);
		JAM_CloseMB(jb);
	}
	free(to);
	free(subject);
}

void list_messages(struct user_record *user) {
	struct msg_headers *msghs;
	s_JamBase *jb;
	int all_unread;
	s_JamLastRead jlr;
	char buffer[256];
	int i;
	int k;
	int j;
	int start;
	int closed;
	int redraw;
	struct tm msg_date;
	char c;

	s_printf("\r\n");
	// list mail in message base
	msghs = read_message_headers(user->cur_mail_conf, user->cur_mail_area, user);
	if (msghs != NULL && msghs->msg_count > 0) {
		jb = open_jam_base(conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
		if (!jb) {
			dolog("Error opening JAM base.. %s", conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
			return;
		} else {
			all_unread = 0;
			if (JAM_ReadLastRead(jb, user->id, &jlr) == JAM_NO_USER) {
				jlr.LastReadMsg = 0;
				jlr.HighReadMsg = 0;
				all_unread = 1;
			}
			JAM_CloseMB(jb);
			s_printf(get_string(125), msghs->msg_count);

			s_readstring(buffer, 6);
			if (tolower(buffer[0]) == 'n') {
				if (all_unread == 0) {
					k = jlr.HighReadMsg;
					for (i=0;i<msghs->msg_count;i++) {
						if (msghs->msgs[i]->msg_h->MsgNum == k) {
							break;
						}
					}
					if (i == msghs->msg_count - 1) {
						i = 1;
					} else {
						i+=2;
					}
							
				} else {
					i = 1;
				}
			} else {
				i = atoi(buffer);
				if (i <= 0) {
					i = 1;
				}
			}
			closed = 0;
							

			redraw = 1;
			start = i-1;
			while (!closed) {
				if (redraw) {
					s_printf(get_string(126));
					for (j=start;j<start + 22 && j<msghs->msg_count;j++) {
						gmtime_r((time_t *)&msghs->msgs[j]->msg_h->DateWritten, &msg_date);
						if (j == i -1) {
							if (msghs->msgs[j]->msg_h->MsgNum > jlr.HighReadMsg || all_unread) {
								s_printf(get_string(188), j + 1, msghs->msgs[j]->subject, msghs->msgs[j]->from, msghs->msgs[j]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
							} else {
								s_printf(get_string(189), j + 1, msghs->msgs[j]->subject, msghs->msgs[j]->from, msghs->msgs[j]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
							}
						} else {
							if (msghs->msgs[j]->msg_h->MsgNum > jlr.HighReadMsg || all_unread) {
								s_printf(get_string(127), j + 1, msghs->msgs[j]->subject, msghs->msgs[j]->from, msghs->msgs[j]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
							} else {
								s_printf(get_string(128), j + 1, msghs->msgs[j]->subject, msghs->msgs[j]->from, msghs->msgs[j]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
							}
						}
					}
					s_printf(get_string(190));
					s_printf("\e[%d;5H", i - start + 1);
					redraw = 0;
				}
				c = s_getchar();
				if (tolower(c) == 'q') {
					closed = 1;
				} else if (c == 27) {
					c = s_getchar();
					if (c == 91) {
						c = s_getchar();
						if (c == 66) {
							// down
							i++;
							if (i > start + 22) {
								start += 22;
								if (start > msghs->msg_count) {
									start = msghs->msg_count - 22;
								}
								redraw = 1;
							}
							if (i-1 == msghs->msg_count) {
								i--;
								s_printf("\e[%d;5H", i - start + 1);
							} else if (!redraw) {
								s_printf("\e[%d;1H", i - start);
								gmtime_r((time_t *)&msghs->msgs[i-2]->msg_h->DateWritten, &msg_date);
								if (msghs->msgs[i-2]->msg_h->MsgNum > jlr.HighReadMsg || all_unread) {
									s_printf(get_string(127), i - 1, msghs->msgs[i-2]->subject, msghs->msgs[i-2]->from, msghs->msgs[i-2]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
								} else {
									s_printf(get_string(128), i - 1, msghs->msgs[i-2]->subject, msghs->msgs[i-2]->from, msghs->msgs[i-2]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
								}												
								s_printf("\e[%d;1H", i - start + 1);
								gmtime_r((time_t *)&msghs->msgs[i-1]->msg_h->DateWritten, &msg_date);
								if (msghs->msgs[i-1]->msg_h->MsgNum > jlr.HighReadMsg || all_unread) {
									s_printf(get_string(188), i, msghs->msgs[i-1]->subject, msghs->msgs[i-1]->from, msghs->msgs[i-1]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
								} else {
									s_printf(get_string(189), i, msghs->msgs[i-1]->subject, msghs->msgs[i-1]->from, msghs->msgs[i-1]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
								}												
								s_printf("\e[%d;5H", i - start + 1);
							}
						} else if (c == 65) {
							// up
							i--;
							if (i - 1 < start) {
								start -=22;
								if (start < 0) {
									start = 0;
								}
								redraw = 1;
							}
							if (i <= 1) {
								start = 0;
								i = 1;
								redraw = 1;
							} else if (!redraw) {
								s_printf("\e[%d;1H", i - start + 2);
								gmtime_r((time_t *)&msghs->msgs[i]->msg_h->DateWritten, &msg_date);
								if (msghs->msgs[i]->msg_h->MsgNum > jlr.HighReadMsg || all_unread) {
									s_printf(get_string(127), i + 1, msghs->msgs[i]->subject, msghs->msgs[i]->from, msghs->msgs[i]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
								} else {
									s_printf(get_string(128), i + 1, msghs->msgs[i]->subject, msghs->msgs[i]->from, msghs->msgs[i]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
								}												
								s_printf("\e[%d;1H", i - start + 1);
								gmtime_r((time_t *)&msghs->msgs[i-1]->msg_h->DateWritten, &msg_date);
								if (msghs->msgs[i-1]->msg_h->MsgNum > jlr.HighReadMsg || all_unread) {
									s_printf(get_string(188), i, msghs->msgs[i-1]->subject, msghs->msgs[i-1]->from, msghs->msgs[i-1]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
								} else {
									s_printf(get_string(189), i, msghs->msgs[i-1]->subject, msghs->msgs[i-1]->from, msghs->msgs[i-1]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
								}									
								s_printf("\e[%d;5H", i - start + 1);			
									
							}											
						} else if (c == 75) {
							// END KEY
							i = msghs->msg_count;
							start = i - 22;
							if (start < 0) {
								start = 0;
							}
							redraw = 1;
						} else if (c == 72) {
							// HOME KEY
							i = 1;
							start = 0;
							redraw = 1;
						}
					}
				} else if (c == 13) {
					redraw = 1;
					read_message(user, msghs, i - 1);
					free_message_headers(msghs);
					msghs = read_message_headers(user->cur_mail_conf, user->cur_mail_area, user);
					jb = open_jam_base(conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
					if (!jb) {
						dolog("Error opening JAM base.. %s", conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
						if (msghs != NULL) {
							free_message_headers(msghs);
						}						
						return;
					} else {
						all_unread = 0;
						if (JAM_ReadLastRead(jb, user->id, &jlr) == JAM_NO_USER) {
							jlr.LastReadMsg = 0;
							jlr.HighReadMsg = 0;
							all_unread = 1;
						}
						JAM_CloseMB(jb);
					}
				}
			}
		}

		if (msghs != NULL) {
			free_message_headers(msghs);
		}
	} else {
		s_printf(get_string(130));
	}
}

void choose_conference(struct user_record *user) {
	int i;
	int j;
	char c;
	char buffer[6];


	s_printf(get_string(131));
	for (i=0;i<conf.mail_conference_count;i++) {
		if (conf.mail_conferences[i]->sec_level <= user->sec_level) {
			s_printf(get_string(132), i, conf.mail_conferences[i]->name);
		}
		if (i != 0 && i % 20 == 0) {
			s_printf(get_string(6));
			c = s_getc();
		}
	}
	s_printf(get_string(133));
	s_readstring(buffer, 5);
	if (tolower(buffer[0]) != 'q') {
		j = atoi(buffer);
		if (j < 0 || j >= conf.mail_conference_count || conf.mail_conferences[j]->sec_level > user->sec_level) {
			s_printf(get_string(134));
		} else {
			s_printf("\r\n");
			user->cur_mail_conf = j;
			user->cur_mail_area = 0;
		}
	}
}

void choose_area(struct user_record *user) {
	int i;
	int j;
	char c;
	char buffer[6];

	s_printf(get_string(135));
	for (i=0;i<conf.mail_conferences[user->cur_mail_conf]->mail_area_count;i++) {
		if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[i]->read_sec_level <= user->sec_level) {
			s_printf(get_string(136), i, conf.mail_conferences[user->cur_mail_conf]->mail_areas[i]->name);
		}
		if (i != 0 && i % 20 == 0) {
			s_printf(get_string(6));
			c = s_getc();
		}
	}
	s_printf(get_string(137));
	s_readstring(buffer, 5);
	if (tolower(buffer[0]) != 'q') {
		j = atoi(buffer);
		if (j < 0 || j >= conf.mail_conferences[user->cur_mail_conf]->mail_area_count || conf.mail_conferences[user->cur_mail_conf]->mail_areas[j]->read_sec_level > user->sec_level) {
			s_printf(get_string(138));
		} else {
			s_printf("\r\n");
			user->cur_mail_area = j;
		}
	}
}

void next_mail_conf(struct user_record *user) {
	int i;
	
	for (i=user->cur_mail_conf;i<conf.mail_conference_count;i++) {
		if (i + 1 == conf.mail_conference_count) {
			i = -1;
		}
		if (conf.mail_conferences[i+1]->sec_level <= user->sec_level) {
			user->cur_mail_conf = i + 1;
			user->cur_mail_area = 0;
			break;
		}
	}
}

void prev_mail_conf(struct user_record *user) {
	int i;
	for (i=user->cur_mail_conf;i>=0;i--) {
		if (i - 1 == -1) {
			i = conf.mail_conference_count;
		}
		if (conf.mail_conferences[i-1]->sec_level <= user->sec_level) {
			user->cur_mail_conf = i - 1;
			user->cur_mail_area = 0;
			break;
		}
	}
}

void next_mail_area(struct user_record *user) {
	int i;
	for (i=user->cur_mail_area;i<conf.mail_conferences[user->cur_mail_conf]->mail_area_count;i++) {
		if (i + 1 == conf.mail_conferences[user->cur_mail_conf]->mail_area_count) {
			i = -1;
		}
		if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[i+1]->read_sec_level <= user->sec_level) {
			user->cur_mail_area = i + 1;
			break;
		}
	}
}

void prev_mail_area(struct user_record *user) {
	int i;
	for (i=user->cur_mail_area;i>=0;i--) {
		if (i - 1 == -1) {
			i = conf.mail_conferences[user->cur_mail_conf]->mail_area_count;
		}
		if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[i-1]->read_sec_level <= user->sec_level) {
			user->cur_mail_area = i - 1;
			break;
		}
	}
}

void mail_scan(struct user_record *user) {
	s_JamBase *jb;
	s_JamBaseHeader jbh;
	s_JamLastRead jlr;
	struct msg_headers *msghs;
	char c;
	int i;
	int j;
	int lines = 0;

	s_printf(get_string(139));
	c = s_getc();

	if (tolower(c) == 'y') {
		for (i=0;i<conf.mail_conference_count;i++) {
			if (conf.mail_conferences[i]->sec_level > user->sec_level) {
				continue;
			}
			s_printf(get_string(140), i, conf.mail_conferences[i]->name);
			lines+=2;
			if (lines == 22) {
				s_printf(get_string(6));
				s_getc();
				lines = 0;
			}
			for (j=0;j<conf.mail_conferences[i]->mail_area_count;j++) {
				if (conf.mail_conferences[i]->mail_areas[j]->read_sec_level > user->sec_level) {
					continue;
				}
				jb = open_jam_base(conf.mail_conferences[i]->mail_areas[j]->path);
				if (!jb) {
					dolog("Unable to open message base");
					continue;
				}
				if (JAM_ReadMBHeader(jb, &jbh) != 0) {
					JAM_CloseMB(jb);
					continue;
				}
				if (JAM_ReadLastRead(jb, user->id, &jlr) == JAM_NO_USER) {
					if (jbh.ActiveMsgs == 0) {
						JAM_CloseMB(jb);
						continue;
					}
					if (conf.mail_conferences[i]->mail_areas[j]->type == TYPE_NETMAIL_AREA) {
						msghs = read_message_headers(i, j, user);
						if (msghs != NULL) {
							if (msghs->msg_count > 0) {
								s_printf(get_string(141), j, conf.mail_conferences[i]->mail_areas[j]->name, msghs->msg_count);
								lines++;
								if (lines == 22) {
									s_printf(get_string(6));
									s_getc();
									lines = 0;
								}
							}
							free_message_headers(msghs);
						}
					} else {
						s_printf(get_string(141), j, conf.mail_conferences[i]->mail_areas[j]->name, jbh.ActiveMsgs);
						lines++;
						if (lines == 22) {
							s_printf(get_string(6));
							s_getc();
							lines = 0;
						}
					}
				} else {
					if (jlr.HighReadMsg < jbh.ActiveMsgs) {
						if (conf.mail_conferences[i]->mail_areas[j]->type == TYPE_NETMAIL_AREA) {
							msghs = read_message_headers(i, j, user);
							if (msghs != NULL) {
								if (msghs->msg_count > 0) {
									if (msghs->msgs[msghs->msg_count-1]->msg_no > jlr.HighReadMsg) {
										s_printf(get_string(141), j, conf.mail_conferences[i]->mail_areas[j]->name, msghs->msgs[msghs->msg_count-1]->msg_h->MsgNum - jlr.HighReadMsg);
										lines++;
										if (lines == 22) {
											s_printf(get_string(6));
											s_getc();
											lines = 0;
										}
									}
								}
								free_message_headers(msghs);
							}
						} else {
							s_printf(get_string(141), j, conf.mail_conferences[i]->mail_areas[j]->name, jbh.ActiveMsgs - jlr.HighReadMsg);
							lines++;
							if (lines == 22) {
								s_printf(get_string(6));
								s_getc();
								lines = 0;
							}
						}
					} else {
						JAM_CloseMB(jb);
						continue;
					}
				}
				JAM_CloseMB(jb);
			}
		}
		s_printf(get_string(6));
		s_getc();
	}
}

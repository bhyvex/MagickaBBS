#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include "jamlib/jam.h"
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"

extern struct bbs_config conf;
extern struct user_record *gUser;
extern int mynode;

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
	char *wwiv_addressee;
	struct fido_addr *dest;
	int j;
	if (rn) {
		myname = (char *)malloc(strlen(user->firstname) + strlen(user->lastname) + 2);
		sprintf(myname, "%s %s", user->firstname, user->lastname);
	} else {
		myname = strdup(user->loginname);
	}
	if (type == NETWORK_WWIV) {
		wwiv_addressee = strdup(addressed_to);
		for (j=1;j<strlen(addressed_to);j++) {
			if (wwiv_addressee[j] == '(' || wwiv_addressee[j] == '#') {
				wwiv_addressee[j-1] = '\0';
				break;
			}
		}

		if (strcasecmp(myname, wwiv_addressee) == 0) {
			// name match
			if (conf.mail_conferences[msgconf]->wwivnode == atoi(address)) {
				free(wwiv_addressee);
				free(myname);
				return 1;
			}
		}
		free(wwiv_addressee);
		free(myname);
		return 0;
	} else if (type == NETWORK_FIDO) {
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
	if (type == NETWORK_WWIV) {
		free(myname);
		return 0;
	} else if (type == NETWORK_FIDO) {
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

			if (jmh.Attribute & MSG_DELETED) {
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

			if (jmh.Attribute & MSG_PRIVATE) {
				if (!msg_is_to(user, jamm->to, jamm->daddress, conf.mail_conferences[msgconf]->nettype, conf.mail_conferences[msgconf]->realnames, msgconf) &&
				    !msg_is_from(user, jamm->from, jamm->oaddress, conf.mail_conferences[msgconf]->nettype, conf.mail_conferences[msgconf]->realnames, msgconf)) {

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

char *external_editor(struct user_record *user, char *to, char *from, char *quote, char *qfrom, char *subject, int email) {
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



	if (conf.external_editor_cmd != NULL) {
		s_printf(get_string(85));
		c = s_getc();
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
				for (i=0;i<strlen(quote);i++) {
					if (quote[i] == '\r') {
						fprintf(fptr, "\r\n");
					} else if (quote[i] == 0x1) {
						continue;
					} else if (quote[i] == '\e' && quote[i + 1] == '[') {
						while (strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", quote[i]) == NULL)
							i++;
					} else {
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

			rundoor(user, conf.external_editor_cmd, conf.external_editor_stdio);

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
	return editor(user, quote, qfrom, email);
}

char *editor(struct user_record *user, char *quote, char *from, int email) {
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
		for (i=0;i<strlen(quote);i++) {
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
	char timestr[17];
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
		localtime_r((time_t *)&msghs->msgs[mailno]->msg_h->DateWritten, &msg_date);
		sprintf(buffer, "%s", asctime(&msg_date));
		buffer[strlen(buffer) - 1] = '\0';
		s_printf(get_string(109), buffer, mailno + 1, msghs->msg_count);
		s_printf(get_string(110), (msghs->msgs[mailno]->msg_h->Attribute & MSG_SENT ? "SENT" : ""));
		s_printf(get_string(111));

		body = (char *)malloc(msghs->msgs[mailno]->msg_h->TxtLen);

		JAM_ReadMsgText(jb, msghs->msgs[mailno]->msg_h->TxtOffset, msghs->msgs[mailno]->msg_h->TxtLen, (char *)body);
		JAM_WriteLastRead(jb, user->id, &jlr);

		if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_WWIV) {
			body2 = (char *)malloc(msghs->msgs[mailno]->msg_h->TxtLen);
			z2 = 0;
			if (body[0] == 4 && body[1] == '0') {
				skip_line = 1;
			} else {
				skip_line = 0;
			}
			for (z=0;z<msghs->msgs[mailno]->msg_h->TxtLen;z++) {
				if (body[z] == '\r') {
					if (body[z+1] == '\n') {
						z++;
					}
					if (body[z+1] == 4 && body[z+2] == '0') {
						skip_line = 1;
					} else {
						body2[z2++] = '\r';
						skip_line = 0;
					}
				} else {
					if (!skip_line) {
						if (body[z] == 3 || body[z] == 4) {
							z++;
						} else {
							body2[z2++] = body[z];
						}
					}
				}
			}

			free(body);
			body = body2;
		} else {
			z2 = msghs->msgs[mailno]->msg_h->TxtLen;
		}

		lines = 0;
		chars = 0;
        
        
        msg_line_count = 0;
        start_line = 0;
        
        // count the number of lines...
        for (z=0;z<z2;z++) {
            if (body[z] == '\r' || chars == 79) {
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
                if (body[z] == '\e') {
                    ansi = z;
                    while (strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", body[z]) == NULL)
                        z++;
                    if (body[z] == 'm') {
                        // do nothing
                    } else if (body[z] == 'C') {
                        chars += atoi(&body[ansi + 2]);
                    } else {
                        i = strlen(body);
                        for (j=ansi;j<i;j++) {
                            body[ansi++] = body[j];
                        }
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
            s_printf("\e[7;1H\e[0J");
            for (z=position;z<msg_line_count;z++) {
                
                s_printf("%s\e[K\r\n", msg_lines[z]);
                
                if (z - position >= 15) {
                    break;
                }
            }
            s_printf(get_string(187));
            s_printf(get_string(186));
            c = s_getc();
            
            if (c == 'r') {
                should_break = 1;
            } else if (c == 'q') {
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
					if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_WWIV) {
						from = (char *)malloc(strlen(user->loginname) + 20);
						sprintf(from, "%s #%d @%d", user->loginname, user->id, conf.mail_conferences[user->cur_mail_conf]->wwivnode);
					} else {
						from = (char *)malloc(strlen(user->loginname) + 1);
						strcpy(from, user->loginname);
					}
				} else {
					if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_WWIV) {
						from = (char *)malloc(strlen(user->loginname) + 23 + strlen(user->firstname));
						sprintf(from, "%s #%d @%d (%s)", user->loginname, user->id, conf.mail_conferences[user->cur_mail_conf]->wwivnode, user->firstname);
					} else {
						from = (char *)malloc(strlen(user->firstname) + strlen(user->lastname) + 2);
						sprintf(from, "%s %s", user->firstname, user->lastname);
					}
				}
				if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_WWIV && conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_ECHOMAIL_AREA) {
					to = (char *)malloc(4);
					strcpy(to, "ALL");
				} else {
					to = (char *)malloc(strlen(buffer) + 1);
					strcpy(to, buffer);
				}
				replybody = external_editor(user, to, from, body, msghs->msgs[mailno]->from, subject, 0);
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
					jmh.DateWritten = time(NULL);
					jmh.Attribute |= MSG_LOCAL;

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



					if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_ECHOMAIL_AREA) {
						jmh.Attribute |= MSG_TYPEECHO;

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

							snprintf(timestr, 16, "%016lx", time(NULL));

							sprintf(buffer, "%d:%d/%d.%d %s", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point,
															&timestr[strlen(timestr) - 8]);

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
										&msghs->msgs[mailno]->msgid[strlen(timestr) - 8]);
							}

							jsf.LoID   = JAMSFLD_REPLYID;
							jsf.HiID   = 0;
							jsf.DatLen = strlen(buffer);
							jsf.Buffer = (char *)buffer;
							JAM_PutSubfield(jsp, &jsf);
							jmh.ReplyCRC = JAM_Crc32(buffer, strlen(buffer));
						}
					} else if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NETMAIL_AREA) {
						jmh.Attribute |= MSG_TYPENET;
						jmh.Attribute |= MSG_PRIVATE;

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

							snprintf(timestr, 16, "%016lx", time(NULL));

							sprintf(buffer, "%d:%d/%d.%d %s", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
															conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point,
															&timestr[strlen(timestr) - 8]);

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
										&msghs->msgs[mailno]->msgid[strlen(timestr) - 8]);
							}

							jsf.LoID   = JAMSFLD_REPLYID;
							jsf.HiID   = 0;
							jsf.DatLen = strlen(buffer);
							jsf.Buffer = (char *)buffer;
							JAM_PutSubfield(jsp, &jsf);
							jmh.ReplyCRC = JAM_Crc32(buffer, strlen(buffer));
						} else if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_WWIV) {
							sprintf(buffer, "%d", atoi(strchr(from, '@') + 1));
							jsf.LoID   = JAMSFLD_DADDRESS;
							jsf.HiID   = 0;
							jsf.DatLen = strlen(buffer);
							jsf.Buffer = (char *)buffer;
							JAM_PutSubfield(jsp, &jsf);
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
						} else if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_ECHOMAIL_AREA) {
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
					doquit = 1;
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

int mail_menu(struct user_record *user) {
	int doquit = 0;
	int domail = 0;
	char c;
	char buffer[256];
	char buffer2[256];
	int i;
	int j;
	int z;
	int k;
	struct msg_headers *msghs;

	s_JamBase *jb;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;
	s_JamSubfield jsf;
	s_JamLastRead jlr;

	struct tm msg_date;

	char *subject;
	char *from;
	char *to;
	char timestr[17];
	char *msg;
	int closed;
	struct fido_addr *from_addr = NULL;
	int wwiv_to;
	struct stat s;
	int do_internal_menu = 0;
	char *lRet;
	lua_State *L;
	int result;
	int sem_fd;
	int all_unread = 0;
	int redraw;
	int start;
	
	if (conf.script_path != NULL) {
		sprintf(buffer, "%s/mailmenu.lua", conf.script_path);
		if (stat(buffer, &s) == 0) {
			L = luaL_newstate();
			luaL_openlibs(L);
			lua_push_cfunctions(L);
			luaL_loadfile(L, buffer);
			do_internal_menu = 0;
			result = lua_pcall(L, 0, 1, 0);
			if (result) {
				dolog("Failed to run script: %s", lua_tostring(L, -1));
				do_internal_menu = 1;
			}
		} else {
			do_internal_menu = 1;
		}
	} else {
		do_internal_menu = 1;
	}

	while (!domail) {
		if (do_internal_menu == 1) {
			s_displayansi("mailmenu");


			s_printf(get_string(119), user->cur_mail_conf, conf.mail_conferences[user->cur_mail_conf]->name, user->cur_mail_area, conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->name, user->timeleft);

			c = s_getc();
		} else {
			lua_getglobal(L, "menu");
			result = lua_pcall(L, 0, 1, 0);
			if (result) {
				dolog("Failed to run script: %s", lua_tostring(L, -1));
				do_internal_menu = 1;
				lua_close(L);
				continue;
			}
			lRet = (char *)lua_tostring(L, -1);
			lua_pop(L, 1);
			c = lRet[0];
		}
		switch(tolower(c)) {
			case 27:
				{
					c = s_getc();
					if (c == 91) {
						c = s_getc();
					}
				}
				break;
			case '!':
				{
					mail_scan(user);
				}
				break;
			case 'd':
				{
					s_printf("\r\n");
					// list mail in message base
					msghs = read_message_headers(user->cur_mail_conf, user->cur_mail_area, user);
					if (msghs != NULL && msghs->msg_count > 0) {
						jb = open_jam_base(conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
						if (!jb) {
							dolog("Error opening JAM base.. %s", conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
							break;
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
				break;
			case 'p':
				{
					if (user->sec_level < conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->write_sec_level) {
						s_printf(get_string(113));
						break;
					}
					if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_WWIV && conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type  == TYPE_ECHOMAIL_AREA) {
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
							break;
						}
					}
					if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NETMAIL_AREA) {
						s_printf(get_string(121));
						s_readstring(buffer2, 32);
						if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_FIDO) {
							from_addr = parse_fido_addr(buffer2);
							if (!from_addr) {
								s_printf(get_string(122));
								break;
							} else {
								if (from_addr->zone == 0 && from_addr->net == 0 && from_addr->node == 0 && from_addr->point == 0) {
									free(from_addr);
									s_printf(get_string(122));
									break;
								}
								s_printf(get_string(123), from_addr->zone, from_addr->net, from_addr->node, from_addr->point);
							}
						} else if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_WWIV) {
							wwiv_to = atoi(buffer2);
							if (wwiv_to == 0) {
								s_printf(get_string(122));
								break;
							} else {
								s_printf(get_string(124), wwiv_to);
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
						break;
					}
					subject = strdup(buffer);

					// post a message
					if (conf.mail_conferences[user->cur_mail_conf]->realnames == 0) {
						from = strdup(user->loginname);
					} else {
						from = (char *)malloc(strlen(user->firstname) + strlen(user->lastname) + 2);
						sprintf(from, "%s %s", user->firstname, user->lastname);
					}
					msg = external_editor(user, to, from, NULL, NULL, subject, 0);

					free(from);

					if (msg != NULL) {
						jb = open_jam_base(conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
						if (!jb) {
							dolog("Error opening JAM base.. %s", conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
							free(msg);
							free(to);
							free(subject);
							break;
						}

						JAM_ClearMsgHeader( &jmh );
						jmh.DateWritten = (uint32_t)time(NULL);
						jmh.Attribute |= MSG_LOCAL;
						if (conf.mail_conferences[user->cur_mail_conf]->realnames == 0) {
							if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_WWIV) {
								sprintf(buffer, "%s #%d @%d", user->loginname, user->id, conf.mail_conferences[user->cur_mail_conf]->wwivnode);
							} else {
								strcpy(buffer, user->loginname);
							}
						} else {
							if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_WWIV) {
								sprintf(buffer, "%s #%d @%d (%s)", user->loginname, user->id, conf.mail_conferences[user->cur_mail_conf]->wwivnode, user->firstname);
							} else {
								sprintf(buffer, "%s %s", user->firstname, user->lastname);
							}
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

						if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_ECHOMAIL_AREA) {
							jmh.Attribute |= MSG_TYPEECHO;

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

								snprintf(timestr, 16, "%016lx", time(NULL));

								sprintf(buffer, "%d:%d/%d.%d %s", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
																conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
																conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
																conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point,
																&timestr[strlen(timestr) - 8]);

								jsf.LoID   = JAMSFLD_MSGID;
								jsf.HiID   = 0;
								jsf.DatLen = strlen(buffer);
								jsf.Buffer = (char *)buffer;
								JAM_PutSubfield(jsp, &jsf);
								jmh.MsgIdCRC = JAM_Crc32(buffer, strlen(buffer));

							}
						} else
						if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NETMAIL_AREA) {
							jmh.Attribute |= MSG_TYPENET;
							jmh.Attribute |= MSG_PRIVATE;
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
								snprintf(timestr, 16, "%016lx", time(NULL));

								sprintf(buffer, "%d:%d/%d.%d %s", conf.mail_conferences[user->cur_mail_conf]->fidoaddr->zone,
																conf.mail_conferences[user->cur_mail_conf]->fidoaddr->net,
																conf.mail_conferences[user->cur_mail_conf]->fidoaddr->node,
																conf.mail_conferences[user->cur_mail_conf]->fidoaddr->point,
																&timestr[strlen(timestr) - 8]);

								jsf.LoID   = JAMSFLD_MSGID;
								jsf.HiID   = 0;
								jsf.DatLen = strlen(buffer);
								jsf.Buffer = (char *)buffer;
								JAM_PutSubfield(jsp, &jsf);
								jmh.MsgIdCRC = JAM_Crc32(buffer, strlen(buffer));
							} else if (conf.mail_conferences[user->cur_mail_conf]->nettype == NETWORK_WWIV) {
								sprintf(buffer, "%d", wwiv_to);
								jsf.LoID   = JAMSFLD_DADDRESS;
								jsf.HiID   = 0;
								jsf.DatLen = strlen(buffer);
								jsf.Buffer = (char *)buffer;
								JAM_PutSubfield(jsp, &jsf);
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
							break;
						}

						if (JAM_AddMessage(jb, &jmh, jsp, (char *)msg, strlen(msg))) {
							dolog("Failed to add message");
						} else {
							if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_NETMAIL_AREA) {
								if (conf.netmail_sem != NULL) {
									sem_fd = open(conf.netmail_sem, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
									close(sem_fd);
								}
							} else if (conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->type == TYPE_ECHOMAIL_AREA) {
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
				break;
			case 'l':
				{
					s_printf("\r\n");
					// list mail in message base
					msghs = read_message_headers(user->cur_mail_conf, user->cur_mail_area, user);
					if (msghs != NULL && msghs->msg_count > 0) {
						jb = open_jam_base(conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
						if (!jb) {
							dolog("Error opening JAM base.. %s", conf.mail_conferences[user->cur_mail_conf]->mail_areas[user->cur_mail_area]->path);
							break;
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
									i+=2;
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
										localtime_r((time_t *)&msghs->msgs[j]->msg_h->DateWritten, &msg_date);
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
									redraw = 0;
								}
								c = s_getc();
								if (tolower(c) == 'q') {
									closed = 1;
								} else if (c == 27) {
									c = s_getc();
									if (c == 91) {
										c = s_getc();
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
											} else if (!redraw) {
												s_printf("\e[%d;1H", i - start);
												localtime_r((time_t *)&msghs->msgs[i-2]->msg_h->DateWritten, &msg_date);
												if (msghs->msgs[i-2]->msg_h->MsgNum > jlr.HighReadMsg || all_unread) {
													s_printf(get_string(127), i - 1, msghs->msgs[i-2]->subject, msghs->msgs[i-2]->from, msghs->msgs[i-2]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
												} else {
													s_printf(get_string(128), i - 1, msghs->msgs[i-2]->subject, msghs->msgs[i-2]->from, msghs->msgs[i-2]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
												}												
												s_printf("\e[%d;1H", i - start + 1);
												localtime_r((time_t *)&msghs->msgs[i-1]->msg_h->DateWritten, &msg_date);
												if (msghs->msgs[i-1]->msg_h->MsgNum > jlr.HighReadMsg || all_unread) {
													s_printf(get_string(188), i, msghs->msgs[i-1]->subject, msghs->msgs[i-1]->from, msghs->msgs[i-1]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
												} else {
													s_printf(get_string(189), i, msghs->msgs[i-1]->subject, msghs->msgs[i-1]->from, msghs->msgs[i-1]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
												}												
												
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
												localtime_r((time_t *)&msghs->msgs[i]->msg_h->DateWritten, &msg_date);
												if (msghs->msgs[i]->msg_h->MsgNum > jlr.HighReadMsg || all_unread) {
													s_printf(get_string(127), i + 1, msghs->msgs[i]->subject, msghs->msgs[i]->from, msghs->msgs[i]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
												} else {
													s_printf(get_string(128), i + 1, msghs->msgs[i]->subject, msghs->msgs[i]->from, msghs->msgs[i]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
												}												
												s_printf("\e[%d;1H", i - start + 1);
												localtime_r((time_t *)&msghs->msgs[i-1]->msg_h->DateWritten, &msg_date);
												if (msghs->msgs[i-1]->msg_h->MsgNum > jlr.HighReadMsg || all_unread) {
													s_printf(get_string(188), i, msghs->msgs[i-1]->subject, msghs->msgs[i-1]->from, msghs->msgs[i-1]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
												} else {
													s_printf(get_string(189), i, msghs->msgs[i-1]->subject, msghs->msgs[i-1]->from, msghs->msgs[i-1]->to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
												}												
												
											}											
										}
									}
								} else if (c == 13) {
									closed = 1;
									read_message(user, msghs, i - 1);
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
				break;
			case 'c':
				{
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
				break;
			case 'a':
				{
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
				break;
			case 'q':
				{
					domail = 1;
				}
				break;
			case 'g':
				{
					doquit = do_logout();
					domail = doquit;
				}
				break;
			case 'e':
				{
					send_email(user);
				}
				break;
			case 'r':
				{
					// Read your email...
					s_printf("\r\n");
					list_emails(user);
				}
				break;
			case '}':
				{
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
				break;
			case '{':
				{
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
				break;
			case ']':
				{
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
				break;
			case '[':
				{
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
				break;
		}
	}
	if (do_internal_menu == 0) {
		lua_close(L);
	}
	return doquit;
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

#if defined(ENABLE_WWW)
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include "bbs.h"
#include "jamlib/jam.h"

#define IN 0
#define OUT 1
extern char * aha(char *input);
extern struct bbs_config conf;


static char *www_wordwrap(char *content, int cutoff);
static char *www_sanitize(char *inp) {
	int i;
	char *result;
	int len = 0;
	for (i=0;i<strlen(inp);i++) {
		if ((inp[i] == '<') || (inp[i] == '>')) {
			len += 4;
		} else if (inp[i] == '&') {
			len += 5;
		} else {
			len++;
		}
	}

	result = (char *)malloc(len + 1);
	memset(result, 0, len + 1);
	len = 0;
	for (i=0;i<strlen(inp);i++) {
		if (inp[i] == '<')  {
			result[len++] = '&';
			result[len++] = 'l';
			result[len++] = 't';
			result[len++] = ';';
		} else if (inp[i] == '>') {
			result[len++] = '&';
			result[len++] = 'g';
			result[len++] = 't';
			result[len++] = ';';			
		} else if (inp[i] == '&') {
			result[len++] = '&';
			result[len++] = 'a';
			result[len++] = 'm';
			result[len++] = 'p';
			result[len++] = ';';
		} else {
			result[len++] = inp[i];
		}
	}

	return result;
}

char *www_msgs_arealist(struct user_record *user) {
	char *page;
	int max_len;
	int len;
	char buffer[4096];
	int i, j;
	
	page = (char *)malloc(4096);
	max_len = 4096;
	len = 0;
	memset(page, 0, 4096);

	sprintf(buffer, "<div class=\"content-header\"><h2>Message Conferences</h2></div>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);
	
	for (i=0;i<conf.mail_conference_count;i++) {
		if (conf.mail_conferences[i]->sec_level <= user->sec_level) {
			sprintf(buffer, "<div class=\"conference-list-item\">%s</div>\n", conf.mail_conferences[i]->name);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}
			strcat(page, buffer);
			len += strlen(buffer);

			for (j=0;j<conf.mail_conferences[i]->mail_area_count; j++) {
				if (conf.mail_conferences[i]->mail_areas[j]->read_sec_level <= user->sec_level) {
					
					if (new_messages(user, i, j) > 0) {
						sprintf(buffer, "<div class=\"area-list-new\"><a href=\"%smsgs/%d/%d/\">%s</a></div>\n", conf.www_url, i, j, conf.mail_conferences[i]->mail_areas[j]->name);
					} else {
						sprintf(buffer, "<div class=\"area-list-item\"><a href=\"%smsgs/%d/%d/\">%s</a></div>\n", conf.www_url, i, j, conf.mail_conferences[i]->mail_areas[j]->name);
					}
					if (len + strlen(buffer) > max_len - 1) {
						max_len += 4096;
						page = (char *)realloc(page, max_len);
					}
					strcat(page, buffer);
					len += strlen(buffer);
				}
			}
		}
	}
	return page;
}

char *www_msgs_messagelist(struct user_record *user, int conference, int area, int skip) {
	struct msg_headers *mhrs;
	char *page;
	int max_len;
	int len;
	char buffer[4096];
	int i;
	struct tm msg_date;
	time_t date;
	s_JamBase *jb;
	s_JamLastRead jlr;
	int skip_f;
	int skip_t;
	char *to;
	char *from;
	char *subject;

	if (conference < 0 || conference >= conf.mail_conference_count || area < 0 || area >= conf.mail_conferences[conference]->mail_area_count) {
		return NULL;
	}
	page = (char *)malloc(4096);
	max_len = 4096;
	len = 0;
	memset(page, 0, 4096);
	
	sprintf(buffer, "<div class=\"content-header\"><h2>%s - %s</h2></div>\n", conf.mail_conferences[conference]->name, conf.mail_conferences[conference]->mail_areas[area]->name);
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);
	
	if (conf.mail_conferences[conference]->mail_areas[area]->type != TYPE_NETMAIL_AREA) {
		sprintf(buffer, "<div class=\"button\"><a href=\"%smsgs/new/%d/%d\">New Message</a></div>\n", conf.www_url, conference, area);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}
		strcat(page, buffer);
		len += strlen(buffer);	
	}
	mhrs = read_message_headers(conference, area, user);
	
	if (mhrs == NULL) {
		sprintf(buffer, "<h3>No Messages</h3>\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}
		strcat(page, buffer);
		len += strlen(buffer);
		return page;
	}
	
	sprintf(buffer, "<div class=\"div-table\">\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);		

	jb = open_jam_base(conf.mail_conferences[conference]->mail_areas[area]->path);
	if (!jb) {
		free(page);
		free_message_headers(mhrs);
		return NULL;
	}
	if (JAM_ReadLastRead(jb, user->id, &jlr) == JAM_NO_USER) {
		jlr.LastReadMsg = 0;
		jlr.HighReadMsg = 0;
	}
	JAM_CloseMB(jb);
	free(jb);
	skip_f = mhrs->msg_count - skip;
	skip_t = mhrs->msg_count - skip - 50;
	if (skip_t < 0) {
		skip_t = 0;
	}
			
	for (i=skip_f -1; i>=skip_t;i--) {
		date = (time_t)mhrs->msgs[i]->msg_h->DateWritten;
		gmtime_r(&date, &msg_date);
		to = www_sanitize(mhrs->msgs[i]->to);
		from = www_sanitize(mhrs->msgs[i]->from);
		subject = www_sanitize(mhrs->msgs[i]->subject);
		if (mhrs->msgs[i]->msg_h->MsgNum > jlr.HighReadMsg) {
			if (conf.date_style == 1) {
				sprintf(buffer, "<div class=\"msg-summary\"><div class=\"msg-summary-id\">%d</div><div class=\"msg-summary-subject\"><a href=\"%smsgs/%d/%d/%d\">%s</a></div><div class=\"msg-summary-from\">%s</div><div class=\"msg-summary-to\">%s</div><div class=\"msg-summary-date\">%.2d:%.2d %.2d-%.2d-%.2d</div></div>\n", mhrs->msgs[i]->msg_no + 1, conf.www_url, conference, area, mhrs->msgs[i]->msg_h->MsgNum, subject, from, to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mon + 1, msg_date.tm_mday, msg_date.tm_year - 100);
			} else {
				sprintf(buffer, "<div class=\"msg-summary\"><div class=\"msg-summary-id\">%d</div><div class=\"msg-summary-subject\"><a href=\"%smsgs/%d/%d/%d\">%s</a></div><div class=\"msg-summary-from\">%s</div><div class=\"msg-summary-to\">%s</div><div class=\"msg-summary-date\">%.2d:%.2d %.2d-%.2d-%.2d</div></div>\n", mhrs->msgs[i]->msg_no + 1, conf.www_url, conference, area, mhrs->msgs[i]->msg_h->MsgNum, subject, from, to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
			}
		} else {
			if (conf.date_style == 1) {
				sprintf(buffer, "<div class=\"msg-summary-seen\"><div class=\"msg-summary-id\">%d</div><div class=\"msg-summary-subject\"><a href=\"%smsgs/%d/%d/%d\">%s</a></div><div class=\"msg-summary-from\">%s</div><div class=\"msg-summary-to\">%s</div><div class=\"msg-summary-date\">%.2d:%.2d %.2d-%.2d-%.2d</div></div>\n", mhrs->msgs[i]->msg_no + 1, conf.www_url, conference, area, mhrs->msgs[i]->msg_h->MsgNum, subject, from, to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mon + 1, msg_date.tm_mday, msg_date.tm_year - 100);
			} else {
				sprintf(buffer, "<div class=\"msg-summary-seen\"><div class=\"msg-summary-id\">%d</div><div class=\"msg-summary-subject\"><a href=\"%smsgs/%d/%d/%d\">%s</a></div><div class=\"msg-summary-from\">%s</div><div class=\"msg-summary-to\">%s</div><div class=\"msg-summary-date\">%.2d:%.2d %.2d-%.2d-%.2d</div></div>\n", mhrs->msgs[i]->msg_no + 1, conf.www_url, conference, area, mhrs->msgs[i]->msg_h->MsgNum, subject, from, to, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
			}
		}

		free(to);
		free(from);
		free(subject);

		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}
		strcat(page, buffer);
		len += strlen(buffer);		
	}
	sprintf(buffer, "</div>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);		
	
	if (skip > 0) {
		if (skip - 50 < 0) {
			sprintf(buffer, "<div class=\"msg-summary-prev\"><a href=\"%smsgs/%d/%d/\">Prev</a></div>\n", conf.www_url, conference, area);
		} else {
			sprintf(buffer, "<div class=\"msg-summary-prev\"><a href=\"%smsgs/%d/%d/?skip=%d\">Prev</a></div>\n", conf.www_url, conference, area, skip - 50);
		}
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}
		strcat(page, buffer);
		len += strlen(buffer);		
	}
	
	if (skip + 50 <= mhrs->msg_count) {
		sprintf(buffer, "<div class=\"msg-summary-next\"><a href=\"%smsgs/%d/%d/?skip=%d\">Next</a></div>\n", conf.www_url, conference, area, skip + 50);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}
		strcat(page, buffer);
		len += strlen(buffer);
	}
	free_message_headers(mhrs);
	return page;			
}

char *www_msgs_messageview(struct user_record *user, int conference, int area, int msg) {
	s_JamBase *jb;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;
	s_JamSubfield jsf;
	s_JamLastRead jlr;
	s_JamBaseHeader jbh;
	
	char *subject = NULL;
	char *from = NULL;
	char *to = NULL;
	char *daddress = NULL;
	char *oaddress = NULL;
	char *msgid = NULL;
	char *replyid = NULL;
	char *body = NULL;
	char *body2 = NULL;
	int z;
	struct tm msg_date;
	time_t date;
	char *page;
	int max_len;
	int len;
	char buffer[4096];	
	int chars;
	int i;
	char *from_s;
	char *subject_s;
	char *to_s;

	char *aha_text;
	char *aha_out;

	if (conference < 0 || conference >= conf.mail_conference_count || area < 0 || area >= conf.mail_conferences[conference]->mail_area_count) {
		return NULL;
	}
	
	if (conf.mail_conferences[conference]->sec_level <= user->sec_level && conf.mail_conferences[conference]->mail_areas[area]->read_sec_level <= user->sec_level) {
		jb = open_jam_base(conf.mail_conferences[conference]->mail_areas[area]->path);
		if (!jb) {	
			return NULL;
		}
		
		JAM_ReadMBHeader(jb, &jbh);
		
		memset(&jmh, 0, sizeof(s_JamMsgHeader));
		z = JAM_ReadMsgHeader(jb, msg - 1, &jmh, &jsp);
		if (z != 0) {
			JAM_CloseMB(jb);
			free(jb);
			return NULL;
		}
		if (jmh.Attribute & JAM_MSG_DELETED) {
			JAM_DelSubPacket(jsp);
			JAM_CloseMB(jb);
			free(jb);
			return NULL;
		}		
		
		for (z=0;z<jsp->NumFields;z++) {
			if (jsp->Fields[z]->LoID == JAMSFLD_SUBJECT) {
				subject = (char *)malloc(jsp->Fields[z]->DatLen + 1);
				memset(subject, 0, jsp->Fields[z]->DatLen + 1);
				memcpy(subject, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
			}
			if (jsp->Fields[z]->LoID == JAMSFLD_SENDERNAME) {
				from = (char *)malloc(jsp->Fields[z]->DatLen + 1);
				memset(from, 0, jsp->Fields[z]->DatLen + 1);
				memcpy(from, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
			}
			if (jsp->Fields[z]->LoID == JAMSFLD_RECVRNAME) {
				to = (char *)malloc(jsp->Fields[z]->DatLen + 1);
				memset(to, 0, jsp->Fields[z]->DatLen + 1);
				memcpy(to, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
			}
			if (jsp->Fields[z]->LoID == JAMSFLD_DADDRESS) {
				daddress = (char *)malloc(jsp->Fields[z]->DatLen + 1);
				memset(daddress, 0, jsp->Fields[z]->DatLen + 1);
				memcpy(daddress, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
			}
			if (jsp->Fields[z]->LoID == JAMSFLD_OADDRESS) {
				oaddress = (char *)malloc(jsp->Fields[z]->DatLen + 1);
				memset(oaddress, 0, jsp->Fields[z]->DatLen + 1);
				memcpy(oaddress, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
			}
			if (jsp->Fields[z]->LoID == JAMSFLD_MSGID) {
				msgid = (char *)malloc(jsp->Fields[z]->DatLen + 1);
				memset(msgid, 0, jsp->Fields[z]->DatLen + 1);
				memcpy(msgid, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
			}
			if (jsp->Fields[z]->LoID == JAMSFLD_REPLYID) {
				replyid = (char *)malloc(jsp->Fields[z]->DatLen + 1);
				memset(replyid, 0, jsp->Fields[z]->DatLen + 1);
				memcpy(replyid, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
			}
		}
		JAM_DelSubPacket(jsp);					
		
		if (subject == NULL) {
			subject = strdup("(No Subject)");
		}

		if (from == NULL) {
			from = strdup("(No Sender)");
		}

		if (to == NULL) {
			to = strdup("(No Recipient)");
		}
		
		if (jmh.Attribute & JAM_MSG_PRIVATE) {
			if (!msg_is_to(user, to, daddress, conf.mail_conferences[conference]->nettype, conf.mail_conferences[conference]->realnames, conference) &&
				    !msg_is_from(user, from, oaddress, conf.mail_conferences[conference]->nettype, conf.mail_conferences[conference]->realnames, conference) &&
					!msg_is_to(user, to, daddress, conf.mail_conferences[conference]->nettype, !conf.mail_conferences[conference]->realnames, conference) &&
				    !msg_is_from(user, from, oaddress, conf.mail_conferences[conference]->nettype, !conf.mail_conferences[conference]->realnames, conference)) {

				if (subject != NULL) {
					free(subject);
				}
				if (from != NULL) {
					free(from);
				}
				if (to != NULL) {
					free(to);
				}
				if (oaddress != NULL) {
					free(oaddress);
				}
				if (daddress != NULL) {
					free(daddress);
				}
				if (msgid != NULL) {
					free(msgid);
				}
				if (replyid != NULL) {
					free(replyid);
				}
				JAM_CloseMB(jb);
				free(jb);
				return NULL;
			}
		}
		body = (char *)malloc(jmh.TxtLen + 1);
		memset(body, 0, jmh.TxtLen + 1);

		JAM_ReadMsgText(jb, jmh.TxtOffset,jmh.TxtLen, (char *)body);

		if (JAM_ReadLastRead(jb, user->id, &jlr) == JAM_NO_USER) {
			jlr.UserCRC = JAM_Crc32(user->loginname, strlen(user->loginname));
			jlr.UserID = user->id;
			jlr.HighReadMsg = msg;
		}

		jlr.LastReadMsg = msg;
		if (jlr.HighReadMsg < msg) {
			jlr.HighReadMsg = msg;
		}	

		JAM_WriteLastRead(jb, user->id, &jlr);
		JAM_CloseMB(jb);
		free(jb);
		
		page = (char *)malloc(4096);
		max_len = 4096;
		len = 0;
		memset(page, 0, 4096);
		
		sprintf(buffer, "<div class=\"content-header\"><a href=\"%smsgs/%d/%d\"><h2>%s - %s</h2></a></div>\n", conf.www_url, conference, area, conf.mail_conferences[conference]->name, conf.mail_conferences[conference]->mail_areas[area]->name);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}
		strcat(page, buffer);
		len += strlen(buffer);
		

		sprintf(buffer, "<div class=\"msg-view-header\">\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
		subject_s = www_sanitize(subject);
		sprintf(buffer, "<div class=\"msg-view-subject\">%s</div>\n", subject_s);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		free(subject_s);
		strcat(page, buffer);
		len += strlen(buffer);

		from_s = www_sanitize(from);
		if (conf.mail_conferences[conference]->mail_areas[area]->type != TYPE_LOCAL_AREA && oaddress != NULL) {
			sprintf(buffer, "<div class=\"msg-view-from\">From: %s (%s)</div>\n", from_s, oaddress);
		} else {
			sprintf(buffer, "<div class=\"msg-view-from\">From: %s</div>\n", from_s);
		}
		free(from_s);

		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
		to_s = www_sanitize(to);
		sprintf(buffer, "<div class=\"msg-view-to\">To: %s</div>\n", to_s);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		free(to_s);
		strcat(page, buffer);
		len += strlen(buffer);
		
		date = (time_t)jmh.DateWritten;
		gmtime_r(&date, &msg_date);
		if (conf.date_style == 1) {
			sprintf(buffer, "<div class=\"msg-view-date\">Date: %.2d:%.2d %.2d-%.2d-%.2d</div>\n", msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mon + 1, msg_date.tm_mday, msg_date.tm_year - 100);
		} else {
			sprintf(buffer, "<div class=\"msg-view-date\">Date: %.2d:%.2d %.2d-%.2d-%.2d</div>\n", msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
		}
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
		
		sprintf(buffer, "</div>\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
		sprintf(buffer, "<div id=\"msgbody\">\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
			
		
		aha_text = (char *)malloc(jmh.TxtLen + 1);

		memcpy(aha_text, body, jmh.TxtLen);
		aha_text[jmh.TxtLen] = '\0';
		aha_out = aha(aha_text);
		while (len + strlen(aha_out) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, aha_out);
		len += strlen(aha_out);			
		
		free(aha_out);
		free(aha_text);
		
		sprintf(buffer, "</div>\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);


		
				sprintf(buffer, "<div class=\"msg-reply-form\">\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
		if (conf.mail_conferences[conference]->mail_areas[area]->write_sec_level <= user->sec_level && conf.mail_conferences[conference]->mail_areas[area]->type != TYPE_NETMAIL_AREA) {
			sprintf(buffer, "<h3>Reply</h3>\n");
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}
			strcat(page, buffer);
			len += strlen(buffer);
			
			sprintf(buffer, "<form action=\"%smsgs/\" method=\"POST\" enctype=\"application/x-www-form-urlencoded\">\n", conf.www_url);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);

			sprintf(buffer, "<input type=\"hidden\" name=\"conference\" value=\"%d\" />\n", conference);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);
			sprintf(buffer, "<input type=\"hidden\" name=\"area\" value=\"%d\" />\n", area);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);		

			sprintf(buffer, "<input type=\"hidden\" name=\"replyid\" value=\"%s\" />\n", msgid);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);

			sprintf(buffer, "To : <input type=\"text\" name=\"recipient\" value=\"%s\" /><br />\n", from);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);

			if (strncasecmp(subject, "re:", 3) == 0) {
				sprintf(buffer, "Subject : <input type=\"text\" name=\"subject\" value=\"%s\" /><br />\n", subject);
			} else {
				sprintf(buffer, "Subject : <input type=\"text\" name=\"subject\" value=\"RE: %s\" /><br />\n", subject);
			}
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);

			sprintf(buffer, "<textarea name=\"body\" rows=\"25\" cols=\"79\" wrap=\"soft\" id=\"replybody\">");
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);

			sprintf(buffer, " %c> ", from[0]);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);

			chars = 0;
			body2 = www_wordwrap(body, 69);

			if (body2 == NULL) {
				body2 = body;
			} else {
				free(body);
			}

			for (i=0;i<strlen(body2);i++) {
				if (body2[i] == '\r') {
					sprintf(buffer, "\n %c> ", from[0]);
					chars = 0;
				} else if (chars == 73) {
					sprintf(buffer, "\n %c> %c", from[0], body2[i]);
					chars = 1;
				} else {
					sprintf(buffer, "%c", body2[i]);
					chars ++;
				}
				if (len + strlen(buffer) > max_len - 1) {
					max_len += 4096;
					page = (char *)realloc(page, max_len);
				}	
				strcat(page, buffer);
				len += strlen(buffer);			
			}
			free(body2);
			sprintf(buffer, "</textarea>\n<br />");
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);


			sprintf(buffer, "<input type=\"submit\" name=\"submit\" value=\"Reply\" />\n<br />");
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);


			sprintf(buffer, "</form>\n");
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);

			sprintf(buffer, "</div>\n");
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);
		}
		if (subject != NULL) {
			free(subject);
		}
		if (from != NULL) {
			free(from);
		}
		if (to != NULL) {
			free(to);
		}
		if (oaddress != NULL) {
			free(oaddress);
		}
		if (daddress != NULL) {
			free(daddress);
		}
		if (msgid != NULL) {
			free(msgid);
		}
		if (replyid != NULL) {
			free(replyid);
		}		
		return page;
	} else {
		return NULL;
	}
}

static char *www_wordwrap(char *content, int cutoff) {
	int len = strlen(content);
	int i;
	int line_count = 0;
	char *last_space = NULL;
	char *ret = (char *)malloc(len + 1);
	int at = 0;
	int extra = 0;
	if (ret == NULL) {
		return NULL;
	}

	for (i=0;i<len;i++) {
		if (content[i] == '\n') {
			continue;
		}
		if (content[i] != '\r') {
			ret[at] = content[i];
			if (content[i] == ' ') {
				last_space = &ret[at];
			}
			at++;
			
		} else {
			ret[at++] = content[i];
		}
		ret[at] = '\0';
		
		if (content[i] == '\r') {
			line_count = 0;
			last_space = NULL;
		} else if (line_count == cutoff) {
			// wrap
			if (last_space != NULL) {
				*last_space = '\r';
				line_count = strlen(&last_space[1]);
				last_space = NULL;
			} else {
				extra++;
				ret = (char *)realloc(ret, strlen(content) + extra + 1);
				if (ret == NULL) {
					return NULL;
				}
				ret[at++] = '\r';
				ret[at] = '\0';
				last_space = NULL;
				line_count = 0;
			}
		} else {
			line_count++;
		}
	}
	return ret;
}

int www_send_msg(struct user_record *user, char *to, char *subj, int conference, int area, char *replyid, char *body) {
	s_JamBase *jb;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;
	s_JamSubfield jsf;
	s_JamLastRead jlr;
	s_JamBaseHeader jbh;
	int z;
	int sem_fd;
	char *page;
	int max_len;
	int len;
	char buffer[256];	
	char *body2;
	char *tagline;
	struct utsname name;
	int pos;
	char *body3;
	
	if (conference < 0 || conference >= conf.mail_conference_count || area < 0 || area >= conf.mail_conferences[conference]->mail_area_count) {
		return 0;
	}
	if (subj == NULL || to == NULL || body == NULL) {
		return 0;
	}
	if (conf.mail_conferences[conference]->mail_areas[area]->write_sec_level <= user->sec_level && conf.mail_conferences[conference]->mail_areas[area]->type != TYPE_NETMAIL_AREA) {
		jb = open_jam_base(conf.mail_conferences[conference]->mail_areas[area]->path);
		if (!jb) {
			return 0;
		}
		
		JAM_ClearMsgHeader( &jmh );
		jmh.DateWritten = (uint32_t)utc_to_local(time(NULL));
		jmh.Attribute |= JAM_MSG_LOCAL;	
		
		if (conf.mail_conferences[conference]->realnames == 0) {
			if (conf.mail_conferences[conference]->nettype == NETWORK_WWIV) {
				sprintf(buffer, "%s #%d @%d", user->loginname, user->id, conf.mail_conferences[conference]->wwivnode);
			} else {
				strcpy(buffer, user->loginname);
			}
		} else {
			if (conf.mail_conferences[conference]->nettype == NETWORK_WWIV) {
				sprintf(buffer, "%s #%d @%d (%s)", user->loginname, user->id, conf.mail_conferences[conference]->wwivnode, user->firstname);
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
		jsf.DatLen = strlen(subj);
		jsf.Buffer = (char *)subj;
		JAM_PutSubfield(jsp, &jsf);
		
		if (conf.mail_conferences[conference]->mail_areas[area]->type == TYPE_ECHOMAIL_AREA) {
			jmh.Attribute |= JAM_MSG_TYPEECHO;

			if (conf.mail_conferences[conference]->nettype == NETWORK_FIDO) {
				if (conf.mail_conferences[conference]->fidoaddr->point) {
					sprintf(buffer, "%d:%d/%d.%d", conf.mail_conferences[conference]->fidoaddr->zone,
												   conf.mail_conferences[conference]->fidoaddr->net,
												   conf.mail_conferences[conference]->fidoaddr->node,
												   conf.mail_conferences[conference]->fidoaddr->point);
				} else {
					sprintf(buffer, "%d:%d/%d", conf.mail_conferences[conference]->fidoaddr->zone,
												conf.mail_conferences[conference]->fidoaddr->net,
												conf.mail_conferences[conference]->fidoaddr->node);
				}
				jsf.LoID   = JAMSFLD_OADDRESS;
				jsf.HiID   = 0;
				jsf.DatLen = strlen(buffer);
				jsf.Buffer = (char *)buffer;
				JAM_PutSubfield(jsp, &jsf);

				sprintf(buffer, "%d:%d/%d.%d %08lx", conf.mail_conferences[conference]->fidoaddr->zone,
												  conf.mail_conferences[conference]->fidoaddr->net,
												  conf.mail_conferences[conference]->fidoaddr->node,
												  conf.mail_conferences[conference]->fidoaddr->point,
												  generate_msgid());

				jsf.LoID   = JAMSFLD_MSGID;
				jsf.HiID   = 0;
				jsf.DatLen = strlen(buffer);
				jsf.Buffer = (char *)buffer;
				JAM_PutSubfield(jsp, &jsf);
				jmh.MsgIdCRC = JAM_Crc32(buffer, strlen(buffer));
				
				if (strcasecmp(replyid, "NULL") != 0) {
					jsf.LoID   = JAMSFLD_REPLYID;
					jsf.HiID   = 0;
					jsf.DatLen = strlen(replyid);
					jsf.Buffer = (char *)replyid;
					JAM_PutSubfield(jsp, &jsf);
					jmh.ReplyCRC = JAM_Crc32(buffer, strlen(replyid));
				}
			}
		}
		while (1) {
			z = JAM_LockMB(jb, 100);
			if (z == 0) {
				break;
			} else if (z == JAM_LOCK_FAILED) {
				sleep(1);
			} else {
				JAM_CloseMB(jb);
				free(jb);
				return 0;
			}
		}
		if (z != 0) {
			JAM_CloseMB(jb);
			free(jb);
			return 0;
		}
		
		if (conf.mail_conferences[conference]->tagline != NULL) {
			tagline = conf.mail_conferences[conference]->tagline;
		} else {
			tagline = conf.default_tagline;
		}		
		
		uname(&name);

		if (conf.mail_conferences[conference]->nettype == NETWORK_FIDO) {
			if (conf.mail_conferences[conference]->fidoaddr->point == 0) {
				snprintf(buffer, 256, "\r\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s (%d:%d/%d)\r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline, conf.mail_conferences[conference]->fidoaddr->zone,
																																						  conf.mail_conferences[conference]->fidoaddr->net,
																																						  conf.mail_conferences[conference]->fidoaddr->node);
			} else {
				snprintf(buffer, 256, "\r\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s (%d:%d/%d.%d)\r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline, conf.mail_conferences[conference]->fidoaddr->zone,
																																						  conf.mail_conferences[conference]->fidoaddr->net,
																																						  conf.mail_conferences[conference]->fidoaddr->node,
																																						  conf.mail_conferences[conference]->fidoaddr->point);
			}
		} else {
			snprintf(buffer, 256, "\r\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s \r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline);
		}
		body2 = www_wordwrap(body, 73);
		if (body2 == NULL) {
			JAM_UnlockMB(jb);
			JAM_DelSubPacket(jsp);
			JAM_CloseMB(jb);
			free(jb);
			return 0;
		}
		body3 = (char *)malloc(strlen(body2) + 2 + strlen(buffer));		
		if (body3 == NULL) {
			free(body2);
			JAM_UnlockMB(jb);
			JAM_DelSubPacket(jsp);
			JAM_CloseMB(jb);
			free(jb);
			return 0;
		}
		
		memset(body3, 0, strlen(body2) + 2 + strlen(buffer));
		pos = 0;
		sprintf(body3, "%s%s", body2, buffer);
		
		if (JAM_AddMessage(jb, &jmh, jsp, (char *)body3, strlen(body3))) {
			free(body3);
			free(body2);
			JAM_UnlockMB(jb);
			JAM_DelSubPacket(jsp);
			JAM_CloseMB(jb);
			free(jb);
			return 0;
		} else {
			if (conf.mail_conferences[conference]->mail_areas[area]->type == TYPE_ECHOMAIL_AREA) {
				if (conf.echomail_sem != NULL) {
					sem_fd = open(conf.echomail_sem, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
					close(sem_fd);
				}
			}
		}

		free(body2);
		free(body3);

		JAM_UnlockMB(jb);

		JAM_DelSubPacket(jsp);
		JAM_CloseMB(jb);		
		free(jb);
		return 1;
	}
	return 0;
}

char *www_new_msg(struct user_record *user, int conference, int area) {
	char *page;
	int max_len;
	int len;
	char buffer[4096];
	
	page = (char *)malloc(4096);
	max_len = 4096;
	len = 0;
	memset(page, 0, 4096);
	
	sprintf(buffer, "<div class=\"content-header\"><h2>New Message</h2></div>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "<form action=\"%smsgs/\" method=\"POST\" onsubmit=\"return validate()\" enctype=\"application/x-www-form-urlencoded\">\n", conf.www_url);
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "<input type=\"hidden\" name=\"conference\" value=\"%d\" />\n", conference);
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);
	sprintf(buffer, "<input type=\"hidden\" name=\"area\" value=\"%d\" />\n", area);
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);		

	sprintf(buffer, "<input type=\"hidden\" name=\"replyid\" value=\"NULL\" />\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "To : <input type=\"text\" name=\"recipient\" value=\"All\" id=\"recipient\"/><br />\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "Subject : <input type=\"text\" name=\"subject\" id=\"subject\" /><br />\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "<textarea name=\"body\" id=\"body\" rows=\"25\" cols=\"79\" wrap=\"soft\"></textarea>\n<br />");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "<input type=\"submit\" name=\"submit\" value=\"Send\" />\n<br />");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);


	sprintf(buffer, "</form>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);
	
	return page;	
}

#endif

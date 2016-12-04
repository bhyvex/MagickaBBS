#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include "../../jamlib/jam.h"
#include "../../inih/ini.h"

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
		}
	}
	return jb;
}

struct fido_addr {
	unsigned short zone;
	unsigned short net;
	unsigned short node;
	unsigned short point;
};

struct fido_addr *parse_fido_addr(const char *str) {
	struct fido_addr *ret = (struct fido_addr *)malloc(sizeof(struct fido_addr));
	int c;
	int state = 0;

	ret->zone = 0;
	ret->net = 0;
	ret->node = 0;
	ret->point = 0;

	for (c=0;c<strlen(str);c++) {
		switch(str[c]) {
			case ':':
				state = 1;
				break;
			case '/':
				state = 2;
				break;
			case '.':
				state = 3;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				{
					switch (state) {
						case 0:
							ret->zone = ret->zone * 10 + (str[c] - '0');
							break;
						case 1:
							ret->net = ret->net * 10 + (str[c] - '0');
							break;
						case 2:
							ret->node = ret->node * 10 + (str[c] - '0');
							break;
						case 3:
							ret->point = ret->point * 10 + (str[c] - '0');
							break;
					}
				}
				break;
			default:
				free(ret);
				return NULL;
		}
	}
	return ret;
}


struct msg_t {
	int echo;
	char *bbs_path;
	char *filename;
	char *jambase;
	char *from;
	char *subject;
	char *origin;
	struct fido_addr *localAddress;
};

static int handler(void* user, const char* section, const char* name,
                   const char* value)
{
	struct msg_t *msg = (struct msg_t *)user;
	
	if (strcasecmp(section, "main") == 0) {
		if (strcasecmp(name, "echomail") == 0) {
			if (strcasecmp(value, "true") == 0) {
				msg->echo = 1;
			} else {
				msg->echo = 0;
			}
		} else if (strcasecmp(name, "BBS Path") == 0) {
			msg->bbs_path = strdup(value);
		} else if (strcasecmp(name, "Message File") == 0) {
			msg->filename = strdup(value);
		} else if (strcasecmp(name, "JAM Base") == 0) {
			msg->jambase = strdup(value);
		} else if (strcasecmp(name, "From") == 0) {
			msg->from = strdup(value);
		} else if (strcasecmp(name, "Subject") == 0) {
			msg->subject = strdup(value);
		} else if (strcasecmp(name, "Local AKA") == 0) {
			msg->localAddress = parse_fido_addr(value);
		} else if (strcasecmp(name, "Origin Line") == 0) {
			msg->origin = strdup(value);
		}
	}
	return 1;
}

unsigned long generate_msgid(char *bbs_path) {
	time_t theTime;
	
	char buffer[1024];
	
	struct tm timeStruct;
	struct tm fileStruct;
	unsigned long m;
	unsigned long y;
	unsigned long ya;
	unsigned long j;
	unsigned long msgid;
	unsigned long c;
	unsigned long d;
	time_t lastread;
	unsigned long lastid;
	FILE *fptr;
	
	theTime = time(NULL);
	localtime_r(&theTime, &timeStruct);
	
	m = timeStruct.tm_mon + 1;
	y = timeStruct.tm_year + 1900;
	d = timeStruct.tm_mday;
	
	if (m > 2) {
		m = m - 3;
	} else {
		m = m + 9;
		y = y - 1;
	}
	
	c = y / 100;
	ya = y - 100 * c;
	j = (146097 * c) / 4 + (1461 * ya) / 4 + (153 * m + 2) / 5 + d + 1721119;
	
	msgid = (j % 0x800) * 0x200000;
	
	snprintf(buffer, 1024, "%s/msgserial", bbs_path);
	
	fptr = fopen(buffer, "r+");
	if (fptr) {
		flock(fileno(fptr), LOCK_EX);
		fread(&lastread, sizeof(time_t), 1, fptr);
		fread(&lastid, sizeof(unsigned long), 1, fptr);
		localtime_r(&lastread, &fileStruct);
		
		
		if (fileStruct.tm_mon != timeStruct.tm_mon || fileStruct.tm_mday != timeStruct.tm_mday || fileStruct.tm_year != timeStruct.tm_year) {
			lastread = time(NULL);
			lastid = 1;
		} else {
			lastid++;
		}
		rewind(fptr);
		fwrite(&lastread, sizeof(time_t), 1, fptr);
		fwrite(&lastid, sizeof(unsigned long), 1, fptr);
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
	} else {
		fptr = fopen(buffer, "w");
		if (fptr) {
			lastread = time(NULL);
			lastid = 1;
			flock(fileno(fptr), LOCK_EX);
			fwrite(&lastread, sizeof(time_t), 1, fptr);
			fwrite(&lastid, sizeof(unsigned long), 1, fptr);
			flock(fileno(fptr), LOCK_UN);
			fclose(fptr);
		} else {
			printf("Unable to open message id log\n");
			return 0;
		}
	}
	
	msgid += lastid;
	
	return msgid;
}

int main(int argc, char **argv) {
	char buffer[1024];
	char *body;
	char *subject;
	char *from;
	FILE *fptr;
	int len;
	int totlen;
	time_t thetime;
	int z;
	int i;

	struct msg_t msg;

	if (argc < 2) {
		printf("Usage: %s inifile\n", argv[0]);
		exit(1);
	}

	if (ini_parse(argv[1], handler, &msg) <0) {
		fprintf(stderr, "Unable to load configuration ini (%s)!\n", argv[1]);
		exit(-1);
	}

	s_JamBase *jb;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;
	s_JamSubfield jsf;

	fptr = fopen(msg.filename, "r");

	if (!fptr) {
		printf("Unable to open %s\n", msg.filename);
		exit(1);
	}
	body = NULL;
	totlen = 0;

	len = fread(buffer, 1, 1024, fptr);
	while (len > 0) {
		totlen += len;
		if (body == NULL) {
			body = (char *)malloc(totlen + 1);
		} else {
			body = (char *)realloc(body, totlen + 1);
		}
		memcpy(&body[totlen - len], buffer, len);
		body[totlen] = '\0';
		len = fread(buffer, 1, 1024, fptr);
	}

	fclose(fptr);

	for (i=0;i<totlen;i++) {
		if (body[i] == '\n') {
			body[i] = '\r';
		}
	}
	
	if (msg.localAddress->point == 0) {
		snprintf(buffer, 1024, "\r--- mgpost\r * Origin: %s (%d:%d/%d)\r", msg.origin, msg.localAddress->zone, msg.localAddress->net, msg.localAddress->node);
	} else {
		snprintf(buffer, 1024, "\r--- mgpost\r * Origin: %s (%d:%d/%d.%d)\r", msg.origin, msg.localAddress->zone, msg.localAddress->net, msg.localAddress->node, msg.localAddress->point);
	}

	totlen += strlen(buffer);

	body = (char *)realloc(body, totlen + 1);
	
	memcpy(&body[totlen - strlen(buffer)], buffer, strlen(buffer));
	body[totlen] = '\0';

	jb = open_jam_base(msg.jambase);
	if (!jb) {
		printf("Unable to open JAM base %s\n", msg.jambase);
		exit(1);
	}
	thetime = time(NULL);

	JAM_ClearMsgHeader( &jmh );
	jmh.DateWritten = thetime;

	jmh.Attribute |= MSG_LOCAL;

	if (!msg.echo) {
		jmh.Attribute |= MSG_TYPELOCAL;
	} else {
		jmh.Attribute |= MSG_TYPEECHO;
	}

	jsp = JAM_NewSubPacket();
	jsf.LoID   = JAMSFLD_SENDERNAME;
	jsf.HiID   = 0;
	jsf.DatLen = strlen(msg.from);
	jsf.Buffer = (char *)msg.from;
	JAM_PutSubfield(jsp, &jsf);

	jsf.LoID   = JAMSFLD_RECVRNAME;
	jsf.HiID   = 0;
	jsf.DatLen = 3;
	jsf.Buffer = "ALL";
	JAM_PutSubfield(jsp, &jsf);

	jsf.LoID   = JAMSFLD_SUBJECT;
	jsf.HiID   = 0;
	jsf.DatLen = strlen(msg.subject);
	jsf.Buffer = (char *)msg.subject;
	JAM_PutSubfield(jsp, &jsf);

	if (msg.echo) {
		if (msg.localAddress->point == 0) {
			sprintf(buffer, "%d:%d/%d", msg.localAddress->zone, msg.localAddress->net, msg.localAddress->node);
		} else {
			sprintf(buffer, "%d:%d/%d.%d", msg.localAddress->zone, msg.localAddress->net, msg.localAddress->node, msg.localAddress->point);
		}
		
		jsf.LoID   = JAMSFLD_OADDRESS;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(buffer);
		jsf.Buffer = (char *)buffer;
		JAM_PutSubfield(jsp, &jsf);
		
		sprintf(buffer, "%d:%d/%d.%d %08lx", msg.localAddress->zone,
												msg.localAddress->net,
												msg.localAddress->node,
												msg.localAddress->point,
												generate_msgid(msg.bbs_path));

		jsf.LoID   = JAMSFLD_MSGID;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(buffer);
		jsf.Buffer = (char *)buffer;
		JAM_PutSubfield(jsp, &jsf);		
	}

	while (1) {
		z = JAM_LockMB(jb, 100);
		if (z == 0) {
			break;
		} else if (z == JAM_LOCK_FAILED) {
			sleep(1);
		} else {
			printf("Failed to lock msg base!\n");
			break;
		}
	}
	if (z == 0) {
		if (JAM_AddMessage(jb, &jmh, jsp, (char *)body, strlen(body))) {
			printf("Failed to add message\n");
		}

		JAM_UnlockMB(jb);
		JAM_DelSubPacket(jsp);
	}
	JAM_CloseMB(jb);

	return 0;
}

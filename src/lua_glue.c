#include <sys/utsname.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "jamlib/jam.h"

extern int mynode;
extern struct bbs_config conf;

extern struct user_record *gUser;

int l_bbsWString(lua_State *L) {
	char *str = (char *)lua_tostring(L, -1);

	s_printf("%s", str);

	return 0;
}

int l_bbsRString(lua_State *L) {
	char buffer[256];
	int len = lua_tonumber(L, -1);

	if (len > 256) {
		len = 256;
	}

	s_readstring(buffer, len);

	lua_pushstring(L, buffer);

	return 1;
}

int l_bbsRChar(lua_State *L) {
	char c;

	c = s_getc();

	lua_pushlstring(L, &c, 1);

	return 1;
}

int l_bbsDisplayAnsiPause(lua_State *L) {
	char *str = (char *)lua_tostring(L, -1);
	char buffer[256];
	
	if (strchr(str, '/') == NULL) {
		sprintf(buffer, "%s/%s.ans", conf.ansi_path, str);
		s_displayansi_pause(buffer, 1);
	} else {
		s_displayansi_pause(str, 1);
	}
	return 0;
}

int l_bbsDisplayAnsi(lua_State *L) {
	char *str = (char *)lua_tostring(L, -1);

	s_displayansi(str);

	return 0;
}

int l_bbsVersion(lua_State *L) {
	char buffer[64];
	snprintf(buffer, 64, "Magicka BBS v%d.%d (%s)", VERSION_MAJOR, VERSION_MINOR, VERSION_STR);

	lua_pushstring(L, buffer);

	return 1;
}

int l_bbsNode(lua_State *L) {
	lua_pushnumber(L, mynode);

	return 1;
}

int l_bbsReadLast10(lua_State *L) {
	int offset = lua_tonumber(L, -1);
	struct last10_callers l10;
	FILE *fptr;

	fptr = fopen("last10.dat", "rb");
	if (!fptr) {
		return 0;
	}
	fseek(fptr, offset * sizeof(struct last10_callers), SEEK_SET);
	if (fread(&l10, sizeof(struct last10_callers), 1, fptr) == 0) {
		return 0;
	}
	fclose(fptr);

	lua_pushstring(L, l10.name);
	lua_pushstring(L, l10.location);
	lua_pushnumber(L, l10.time);

	return 3;
}

int l_bbsGetEmailCount(lua_State *L) {
	lua_pushnumber(L, mail_getemailcount(gUser));

	return 1;
}

int l_bbsFullMailScan(lua_State *L) {
	full_mail_scan(gUser);
	return 0;
}

int l_bbsMailScan(lua_State *L) {
	mail_scan(gUser);
	return 0;
}

int l_bbsFileScan(lua_State *L) {
	file_scan();
	return 0;
}

int l_bbsRunDoor(lua_State *L) {
	char *cmd = (char *)lua_tostring(L, 1);
	int stdio = lua_toboolean(L, 2);
	char *codepage = (char *)lua_tostring(L, 3);
	rundoor(gUser, cmd, stdio, codepage);

	return 0;
}

int l_bbsTimeLeft(lua_State *L) {
	lua_pushnumber(L, gUser->timeleft);

	return 1;
}

int l_bbsDisplayAutoMsg(lua_State *L) {
	automessage_display();
	return 0;
}

int l_getMailAreaInfo(lua_State *L) {
	lua_pushnumber(L, gUser->cur_mail_conf);
	lua_pushstring(L, conf.mail_conferences[gUser->cur_mail_conf]->name);
	lua_pushnumber(L, gUser->cur_mail_area);
	lua_pushstring(L, conf.mail_conferences[gUser->cur_mail_conf]->mail_areas[gUser->cur_mail_area]->name);

	return 4;
}

int l_getFileAreaInfo(lua_State *L) {
	lua_pushnumber(L, gUser->cur_file_dir);
	lua_pushstring(L, conf.file_directories[gUser->cur_file_dir]->name);
	lua_pushnumber(L, gUser->cur_file_sub);
	lua_pushstring(L, conf.file_directories[gUser->cur_file_dir]->file_subs[gUser->cur_file_sub]->name);

	return 4;
}

int l_getBBSInfo(lua_State *L) {
	struct utsname name;

	uname(&name);

	lua_pushstring(L, conf.bbs_name);
	lua_pushstring(L, conf.sysop_name);
	lua_pushstring(L, name.sysname);
	lua_pushstring(L, name.machine);

	return 4;
}


int l_getUserHandle(lua_State *L) {	
	lua_pushstring(L, gUser->loginname);

	return 1;
}

int l_messageFound(lua_State *L) {
	int conference = lua_tointeger(L, 1);
	int area = lua_tointeger(L, 2);
	int id = lua_tointeger(L, 3);
	int z;

	s_JamBase *jb;
	s_JamBaseHeader jbh;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;

	jb = open_jam_base(conf.mail_conferences[conference]->mail_areas[area]->path);
	if (!jb) {
		dolog("Error opening JAM base.. %s", conf.mail_conferences[conference]->mail_areas[area]->path);
		lua_pushnumber(L, 0);
		return 1;
	}
	z = JAM_ReadMsgHeader(jb, id, &jmh, &jsp);

	if (z != 0) {
		dolog("Failed to read msg header: %d Erro %d", z, JAM_Errno(jb));
		JAM_CloseMB(jb);
		lua_pushnumber(L, 0);
		return 1;
	}
	if (jmh.Attribute & JAM_MSG_DELETED) {
		JAM_DelSubPacket(jsp);
		JAM_CloseMB(jb);
		lua_pushnumber(L, 0);
		return 1;
	}
	
	JAM_DelSubPacket(jsp);
	JAM_CloseMB(jb);
	
	lua_pushnumber(L, 1);
	return 1;	
}

int l_readMessageHdr(lua_State *L) {
	int conference = lua_tointeger(L, 1);
	int area = lua_tointeger(L, 2);
	int id = lua_tointeger(L, 3);
	int z;

	s_JamBase *jb;
	s_JamBaseHeader jbh;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;

	char *subject = NULL;
	char *sender = NULL;
	char *recipient = NULL;

	jb = open_jam_base(conf.mail_conferences[conference]->mail_areas[area]->path);
	if (!jb) {
		dolog("Error opening JAM base.. %s", conf.mail_conferences[conference]->mail_areas[area]->path);
		return 0;
	}
	z = JAM_ReadMsgHeader(jb, id, &jmh, &jsp);

	if (z != 0) {
		dolog("Failed to read msg header: %d Erro %d", z, JAM_Errno(jb));
		JAM_CloseMB(jb);
	} else if (jmh.Attribute & JAM_MSG_DELETED) {
		JAM_DelSubPacket(jsp);
		JAM_CloseMB(jb);
	} else {
		for (z=0;z<jsp->NumFields;z++) {
			if (jsp->Fields[z]->LoID == JAMSFLD_SUBJECT) {
				subject = (char *)malloc(jsp->Fields[z]->DatLen + 1);
				memset(subject, 0, jsp->Fields[z]->DatLen + 1);
				memcpy(subject, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
			}
			if (jsp->Fields[z]->LoID == JAMSFLD_SENDERNAME) {
				sender = (char *)malloc(jsp->Fields[z]->DatLen + 1);
				memset(sender, 0, jsp->Fields[z]->DatLen + 1);
				memcpy(sender, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
			}
			if (jsp->Fields[z]->LoID == JAMSFLD_RECVRNAME) {
				recipient = (char *)malloc(jsp->Fields[z]->DatLen + 1);
				memset(recipient, 0, jsp->Fields[z]->DatLen + 1);
				memcpy(recipient, jsp->Fields[z]->Buffer, jsp->Fields[z]->DatLen);
			}

		}
		JAM_DelSubPacket(jsp);
		JAM_CloseMB(jb);		
	}
	if (subject == NULL) {
		subject = strdup("(No Subject)");
	}

	if (sender == NULL) {
		sender = strdup("(No Sender)");
	}

	if (recipient == NULL) {
		recipient = strdup("(No Recipient)");
	}

	lua_pushstring(L, sender);
	lua_pushstring(L, recipient);
	lua_pushstring(L, subject);

	free(subject);
	free(sender);
	free(recipient);

	
	return 3;
}

int l_readMessage(lua_State *L) {
	int conference = lua_tointeger(L, 1);
	int area = lua_tointeger(L, 2);
	int id = lua_tointeger(L, 3);
	int z;

	s_JamBase *jb;
	s_JamBaseHeader jbh;
	s_JamMsgHeader jmh;

	char *body = NULL;

	jb = open_jam_base(conf.mail_conferences[conference]->mail_areas[area]->path);
	if (!jb) {
		dolog("Error opening JAM base.. %s", conf.mail_conferences[conference]->mail_areas[area]->path);
		return 0;
	}
	z = JAM_ReadMsgHeader(jb, id, &jmh, NULL);

	if (z != 0) {
		dolog("Failed to read msg header: %d Erro %d", z, JAM_Errno(jb));
		JAM_CloseMB(jb);
		body = strdup("No Message");
	} else if (jmh.Attribute & JAM_MSG_DELETED) {
		JAM_CloseMB(jb);
		body = strdup("No Message");
	} else {
		body = (char *)malloc(jmh.TxtLen + 1);
		JAM_ReadMsgText(jb, jmh.TxtOffset, jmh.TxtLen, (char *)body);
		body[jmh.TxtLen] = '\0';

		JAM_CloseMB(jb);
	}
	lua_pushstring(L, body);

	free(body);
	
	return 1;
}

int l_tempPath(lua_State *L) {
	char buffer[PATH_MAX];
	struct stat s;
	snprintf(buffer, PATH_MAX, "%s/node%d/lua/", conf.bbs_path, mynode);

	if (stat(buffer, &s) != 0) {
		mkdir(buffer, 0755);
	}
	
	lua_pushstring(L, buffer);

	return 1;
}

int l_postMessage(lua_State *L) {
	int confr = lua_tointeger(L, 1);
	int area = lua_tointeger(L, 2);
	time_t dwritten = time(NULL);
	char *to = lua_tostring(L, 3);
	char *from = lua_tostring(L, 4);
	char *subject = lua_tostring(L, 5);
	char *body = lua_tostring(L, 6);
	int sem_fd;

	char buffer[256];

	s_JamBase *jb;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;
	s_JamSubfield jsf;
	int z;
	int j;
	int i;
	char *msg;
	char *tagline;
	struct utsname name;

	jb = open_jam_base(conf.mail_conferences[confr]->mail_areas[area]->path);
	if (!jb) {
		dolog("Error opening JAM base.. %s", conf.mail_conferences[confr]->mail_areas[area]->path);
		return 0;
	}

	JAM_ClearMsgHeader( &jmh );
	jmh.DateWritten = dwritten;
	jmh.Attribute |= JAM_MSG_LOCAL;


	jsp = JAM_NewSubPacket();

	jsf.LoID   = JAMSFLD_SENDERNAME;
	jsf.HiID   = 0;
	jsf.DatLen = strlen(from);
	jsf.Buffer = (char *)from;
	JAM_PutSubfield(jsp, &jsf);

	if (conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_NEWSGROUP_AREA) {
		sprintf(buffer, "ALL");
		jsf.LoID   = JAMSFLD_RECVRNAME;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(buffer);
		jsf.Buffer = (char *)buffer;
		JAM_PutSubfield(jsp, &jsf);

	} else {
		jsf.LoID   = JAMSFLD_RECVRNAME;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(to);
		jsf.Buffer = (char *)to;
		JAM_PutSubfield(jsp, &jsf);
	}

	jsf.LoID   = JAMSFLD_SUBJECT;
	jsf.HiID   = 0;
	jsf.DatLen = strlen(subject);
	jsf.Buffer = (char *)subject;
	JAM_PutSubfield(jsp, &jsf);

	if (conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_ECHOMAIL_AREA || conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_NEWSGROUP_AREA) {
		jmh.Attribute |= JAM_MSG_TYPEECHO;

		if (conf.mail_conferences[confr]->fidoaddr->point) {
			sprintf(buffer, "%d:%d/%d.%d", conf.mail_conferences[confr]->fidoaddr->zone,
											conf.mail_conferences[confr]->fidoaddr->net,
											conf.mail_conferences[confr]->fidoaddr->node,
											conf.mail_conferences[confr]->fidoaddr->point);
		} else {
			sprintf(buffer, "%d:%d/%d", conf.mail_conferences[confr]->fidoaddr->zone,
											conf.mail_conferences[confr]->fidoaddr->net,
											conf.mail_conferences[confr]->fidoaddr->node);
		}
		jsf.LoID   = JAMSFLD_OADDRESS;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(buffer);
		jsf.Buffer = (char *)buffer;
		JAM_PutSubfield(jsp, &jsf);

		sprintf(buffer, "%d:%d/%d.%d %08lx", conf.mail_conferences[confr]->fidoaddr->zone,
												conf.mail_conferences[confr]->fidoaddr->net,
												conf.mail_conferences[confr]->fidoaddr->node,
												conf.mail_conferences[confr]->fidoaddr->point,
												generate_msgid());

		jsf.LoID   = JAMSFLD_MSGID;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(buffer);
		jsf.Buffer = (char *)buffer;
		JAM_PutSubfield(jsp, &jsf);
		jmh.MsgIdCRC = JAM_Crc32(buffer, strlen(buffer));

	} else if (conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_NETMAIL_AREA) {
		JAM_DelSubPacket(jsp);
		JAM_CloseMB(jb);
		return 0;
	}

	while (1) {
		z = JAM_LockMB(jb, 100);
		if (z == 0) {
			break;
		} else if (z == JAM_LOCK_FAILED) {
			sleep(1);
		} else {
			dolog("Failed to lock msg base!");
			JAM_CloseMB(jb);			
			return 0;
		}
	}

	uname(&name);
	if (conf.mail_conferences[confr]->tagline != NULL) {
		tagline = conf.mail_conferences[confr]->tagline;
	} else {
		tagline = conf.default_tagline;
	}

	if (conf.mail_conferences[confr]->nettype == NETWORK_FIDO) {
		if (conf.mail_conferences[confr]->fidoaddr->point == 0) {
			snprintf(buffer, 256, "\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s (%d:%d/%d)\r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline, conf.mail_conferences[confr]->fidoaddr->zone,
																																							conf.mail_conferences[confr]->fidoaddr->net,
																																							conf.mail_conferences[confr]->fidoaddr->node);
		} else {
			snprintf(buffer, 256, "\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s (%d:%d/%d.%d)\r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline, conf.mail_conferences[confr]->fidoaddr->zone,
																																							conf.mail_conferences[confr]->fidoaddr->net,
																																							conf.mail_conferences[confr]->fidoaddr->node,
																																							conf.mail_conferences[confr]->fidoaddr->point);
		}
	} else {
		snprintf(buffer, 256, "\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s \r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, tagline);
	}

	msg = (char *)malloc(strlen(body) + 2 + strlen(buffer));
	
	j = 0;

	for (i=0;i<strlen(body);i++) {
		if (body[i] == '\n') {
			continue;
		}
		msg[j++] = body[i];
		msg[j] = '\0';
	}

	strcat(msg, buffer);

	if (JAM_AddMessage(jb, &jmh, jsp, (char *)msg, strlen(msg))) {
		dolog("Failed to add message");
		JAM_UnlockMB(jb);
		
		JAM_DelSubPacket(jsp);
		JAM_CloseMB(jb);
		free(msg);
		return 0;
	} else {
		JAM_UnlockMB(jb);

		JAM_DelSubPacket(jsp);
		JAM_CloseMB(jb);
	}
	free(msg);

	if (conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_ECHOMAIL_AREA || conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_NEWSGROUP_AREA) {
		if (conf.echomail_sem != NULL) {
			sem_fd = open(conf.echomail_sem, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
			close(sem_fd);
		}
	}

	return 0;
}

void lua_push_cfunctions(lua_State *L) {
	lua_pushcfunction(L, l_bbsWString);
	lua_setglobal(L, "bbs_write_string");
	lua_pushcfunction(L, l_bbsRString);
	lua_setglobal(L, "bbs_read_string");
	lua_pushcfunction(L, l_bbsDisplayAnsiPause);
	lua_setglobal(L, "bbs_display_ansi_pause");	
	lua_pushcfunction(L, l_bbsDisplayAnsi);
	lua_setglobal(L, "bbs_display_ansi");
	lua_pushcfunction(L, l_bbsRChar);
	lua_setglobal(L, "bbs_read_char");
	lua_pushcfunction(L, l_bbsVersion);
	lua_setglobal(L, "bbs_version");
	lua_pushcfunction(L, l_bbsNode);
	lua_setglobal(L, "bbs_node");
	lua_pushcfunction(L, l_bbsReadLast10);
	lua_setglobal(L, "bbs_read_last10");
	lua_pushcfunction(L, l_bbsGetEmailCount);
	lua_setglobal(L, "bbs_get_emailcount");
	lua_pushcfunction(L, l_bbsMailScan);
	lua_setglobal(L, "bbs_mail_scan");
	lua_pushcfunction(L, l_bbsRunDoor);
	lua_setglobal(L, "bbs_run_door");
	lua_pushcfunction(L, l_bbsTimeLeft);
	lua_setglobal(L, "bbs_time_left");
	lua_pushcfunction(L, l_getMailAreaInfo);
	lua_setglobal(L, "bbs_cur_mailarea_info");
	lua_pushcfunction(L, l_getFileAreaInfo);
	lua_setglobal(L, "bbs_cur_filearea_info");
	lua_pushcfunction(L, l_bbsDisplayAutoMsg);
	lua_setglobal(L, "bbs_display_automsg");
	lua_pushcfunction(L, l_getBBSInfo);
	lua_setglobal(L, "bbs_get_info");
	lua_pushcfunction(L, l_bbsFileScan);
	lua_setglobal(L, "bbs_file_scan");	
	lua_pushcfunction(L, l_bbsFullMailScan);
	lua_setglobal(L, "bbs_full_mail_scan");	
	lua_pushcfunction(L, l_getUserHandle);
	lua_setglobal(L, "bbs_get_userhandle");	
	lua_pushcfunction(L, l_messageFound);
	lua_setglobal(L, "bbs_message_found");	
	lua_pushcfunction(L, l_readMessageHdr);
	lua_setglobal(L, "bbs_read_message_hdr");	
	lua_pushcfunction(L, l_readMessage);
	lua_setglobal(L, "bbs_read_message");
	lua_pushcfunction(L, l_tempPath);
	lua_setglobal(L, "bbs_temp_path");
	lua_pushcfunction(L, l_postMessage);
	lua_setglobal(L, "bbs_post_message");
}

void do_lua_script(char *script) {
	lua_State *L;
	char buffer[PATH_MAX];

	if (script == NULL) {
		return;
	}

	if (script[0] == '/') {
		snprintf(buffer, PATH_MAX, "%s.lua", script);
	} else {
		snprintf(buffer, PATH_MAX, "%s/%s.lua", conf.script_path, script);
	}

	L = luaL_newstate();
	luaL_openlibs(L);
	lua_push_cfunctions(L);
	luaL_dofile(L, buffer);
	lua_close(L);
}

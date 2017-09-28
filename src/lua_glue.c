#include <sys/utsname.h>
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lauxlib.h"

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

	sprintf(buffer, "%s/%s.ans", conf.ansi_path, str);
	
	s_displayansi_pause(buffer, 1);

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

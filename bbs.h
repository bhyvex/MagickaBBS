#ifndef __BBS_H__
#define __BBS_H__

#include <time.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 2
#define VERSION_STR "alpha"

#define NETWORK_FIDO 1
#define NETWORK_WWIV 2

#define TYPE_LOCAL_AREA    0
#define TYPE_NETMAIL_AREA  1
#define TYPE_ECHOMAIL_AREA 2

struct fido_addr {
	unsigned short zone;
	unsigned short net;
	unsigned short node;
	unsigned short point;
};

struct last10_callers {
	char name[17];
	char location[33];
	time_t time;
}__attribute__((packed));

struct text_file {
	char *name;
	char *path;
};

struct door_config {
	char *name;
	char key;
	char *command;
	int stdio;
};

struct mail_area {
	char *name;
	char *path;
	int read_sec_level;
	int write_sec_level;
	int type;
};

struct mail_conference {
	char *name;
	char *path;
	char *tagline;
	int networked;
	int nettype;
	int realnames;
	int sec_level;
	int mail_area_count;
	struct mail_area **mail_areas;
	struct fido_addr *fidoaddr;
	int wwivnode;
};

struct file_sub {
	char *name;
	char *database;
	char *upload_path;
	int upload_sec_level;
	int download_sec_level;
};

struct file_directory {
	char *name;
	char *path;
	int sec_level;
	int file_sub_count;
	struct file_sub **file_subs;
};

struct bbs_config {
	char *bbs_name;
	char *sysop_name;
	char *pid_file;
	char *ansi_path;
	char *bbs_path;
	char *log_path;
	char *script_path;
	char *echomail_sem;
	char *netmail_sem;
	char *default_tagline;
	int telnet_port;

	int ssh_server;
	int ssh_port;
	char *ssh_dsa_key;
	char *ssh_rsa_key;

	char *irc_server;
	int irc_port;
	char *irc_channel;

	char *external_editor_cmd;
	int external_editor_stdio;
	int fork;

	int nodes;
	int newuserlvl;
	int automsgwritelvl;
	int mail_conference_count;
	struct mail_conference **mail_conferences;
	int door_count;
	struct door_config **doors;
	int file_directory_count;
	struct file_directory **file_directories;
	int text_file_count;
	struct text_file **text_files;
};

struct sec_level_t {
	int timeperday;
};

struct user_record {
	int id;
	char *loginname;
	char *password;
	char *salt;
	char *firstname;
	char *lastname;
	char *email;
	char *location;
	int sec_level;
	struct sec_level_t *sec_info;
	time_t laston;
	int timeleft;
	int cur_mail_conf;
	int cur_mail_area;
	int cur_file_dir;
	int cur_file_sub;
	int timeson;
};

extern void automessage_write(struct user_record *user);
extern void automessage_display();
extern void dolog(char *fmt, ...);
extern void runbbs_ssh(char *ipaddress);
extern void runbbs(int sock, char *ipaddress);
extern struct fido_addr *parse_fido_addr(const char *str);
extern void s_putchar(char c);
extern void s_printf(char *fmt, ...);
extern void s_putstring(char *c);
extern void s_displayansi_p(char *file);
extern void s_displayansi(char *file);
extern char s_getchar();
extern void s_readpass(char *buffer, int max);
extern void s_readstring(char *buffer, int max);
extern char s_getc();
extern void disconnect(char *calledby);
extern void display_info();
extern void display_last10_callers(struct user_record *user);

extern void gen_salt(char **s);
extern char *hash_sha256(char *pass, char *salt);
extern int save_user(struct user_record *user);
extern int check_user(char *loginname);
extern struct user_record *new_user();
extern struct user_record *check_user_pass(char *loginname, char *password);
extern void list_users(struct user_record *user);

extern void main_menu(struct user_record *user);

extern void mail_scan(struct user_record *user);
extern int mail_menu(struct user_record *user);
extern char *editor(struct user_record *user, char *quote, char *from, int email);
extern char *external_editor(struct user_record *user, char *to, char *from, char *quote, char *qfrom, char *subject, int email);

extern int door_menu(struct user_record *user);
extern void rundoor(struct user_record *user, char *cmd, int stdio);

extern void bbs_list(struct user_record *user);

extern void chat_system(struct user_record *user);

extern int mail_getemailcount(struct user_record *user);
extern void send_email(struct user_record *user);
extern void list_emails(struct user_record *user);

extern int file_menu(struct user_record *user);

extern void settings_menu(struct user_record *user);

extern void lua_push_cfunctions(lua_State *L);
#endif

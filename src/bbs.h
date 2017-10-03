#ifndef __BBS_H__
#define __BBS_H__

#include <time.h>
#include <termios.h>

#if defined(ENABLE_WWW)
#include <microhttpd.h>
#endif
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "jamlib/jam.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 7
#define VERSION_STR "alpha"

#define NETWORK_FIDO 1
#define NETWORK_WWIV 2

#define TYPE_LOCAL_AREA    0
#define TYPE_NETMAIL_AREA  1
#define TYPE_ECHOMAIL_AREA 2
#define TYPE_NEWSGROUP_AREA 3

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
	char *command;
	int stdio;
	char *codepage;
};

struct mail_area {
	char *name;
	char *path;
	char *qwkname;
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

struct archiver {
	char *name;
	char *extension;
	char *unpack;
	char *pack;
};

struct protocol {
	char *name;
	char *upload;
	char *download;
	int internal_zmodem;
	int stdio;
	int upload_prompt;
};

#define IP_STATUS_UNKNOWN 		0
#define IP_STATUS_WHITELISTED 	1
#define IP_STATUS_BLACKLISTED 	2

struct ip_address_guard {
	int status;
	time_t last_connection;
	int connection_count;
};

struct bbs_config {
	int codepage;
	char *bbs_name;
	char *bwave_name;
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
	int www_server;
	int www_port;
	char *www_path;
	int ssh_server;
	int ssh_port;
	char *ssh_dsa_key;
	char *ssh_rsa_key;
	char *string_file;
	char *mgchat_server;
	int mgchat_port;
	char *mgchat_bbstag;
	int bwave_max_msgs;
	int date_style;
	struct fido_addr *main_aka;
	
	char *root_menu;
	char *menu_path;
	char *external_editor_cmd;
	int external_editor_stdio;
	char *external_editor_codepage;
	int fork;
	
	int nodes;
	int newuserlvl;
	int automsgwritelvl;
	int broadcast_enable;
	int broadcast_port;
	char *broadcast_address;
	
	int ipguard_enable;
	int ipguard_timeout;
	int ipguard_tries;
	
	int mail_conference_count;
	struct mail_conference **mail_conferences;
	int door_count;
	struct door_config **doors;
	int file_directory_count;
	struct file_directory **file_directories;
	int text_file_count;
	struct text_file **text_files;
	
	char *config_path;
	int archiver_count;
	struct archiver **archivers;
	
	int protocol_count;
	struct protocol **protocols;
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
	int bwavepktno;
	int defarchiver;
	int defprotocol;
	int nodemsgs;
	int codepage;
	int exteditor;
};

struct jam_msg {
	int msg_no;
	s_JamMsgHeader *msg_h;
	char *from;
	char *to;
	char *subject;
	char *oaddress;
	char *daddress;
	char *msgid;
	char *replyid;
};

struct msg_headers {
	struct jam_msg **msgs;
	int msg_count;
};
extern int copy_file(char *src, char *dest);
extern int recursive_delete(const char *dir);
extern void automessage_write(struct user_record *user);
extern void automessage_display();
extern void dolog(char *fmt, ...);
extern void runbbs_ssh(char *ipaddress);
extern void runbbs(int sock, char *ipaddress);
extern struct fido_addr *parse_fido_addr(const char *str);
extern void s_putchar(char c);
extern void s_printf(char *fmt, ...);
extern void s_putstring(char *c);
extern void s_displayansi_pause(char *file, int pause);
extern void s_displayansi_p(char *file);
extern void s_displayansi(char *file);
extern char s_getchar();
extern void s_readpass(char *buffer, int max);
extern void s_readstring(char *buffer, int max);
extern char s_getc();
extern void disconnect(char *calledby);
extern void display_info();
extern void display_last10_callers(struct user_record *user);
extern void do_logout();

extern void gen_salt(char **s);
extern char *hash_sha256(char *pass, char *salt);
extern int save_user(struct user_record *user);
extern int check_user(char *loginname);
extern struct user_record *new_user();
extern struct user_record *check_user_pass(char *loginname, char *password);
extern void list_users(struct user_record *user);
extern int msgbase_sub_unsub(int conference, int msgbase);
extern int msgbase_is_subscribed(int conference, int msgbase);

extern void active_nodes();
extern void send_node_msg();
extern void display_bulletins();
extern void display_textfiles();

extern time_t utc_to_local(time_t utc);
extern s_JamBase *open_jam_base(char *path);
extern void free_message_headers(struct msg_headers *msghs);
extern struct msg_headers *read_message_headers(int msgconf, int msgarea, struct user_record *user);
extern void mail_scan(struct user_record *user);
extern char *editor(struct user_record *user, char *quote, int qlen, char *from, int email);
extern char *external_editor(struct user_record *user, char *to, char *from, char *quote, int qlen, char *qfrom, char *subject, int email);
extern int msg_is_to(struct user_record *user, char *addressed_to, char *address, int type, int rn, int msgconf);
extern int msg_is_from(struct user_record *user, char *addressed_from, char *address, int type, int rn, int msgconf);
extern unsigned long generate_msgid();
extern void read_mail(struct user_record *user);
extern void list_messages(struct user_record *user);
extern void choose_conference(struct user_record *user);
extern void choose_area(struct user_record *user);
extern void next_mail_conf(struct user_record *user);
extern void prev_mail_conf(struct user_record *user);
extern void next_mail_area(struct user_record *user);
extern void prev_mail_area(struct user_record *user);
extern void post_message(struct user_record *user);
extern void msg_conf_sub_bases();
extern void msgbase_reset_pointers(int conference, int msgarea);
extern void msgbase_reset_all_pointers();

extern void rundoor(struct user_record *user, char *cmd, int stdio, char *codepage);
extern void runexternal(struct user_record *user, char *cmd, int stdio, char **argv, char *cwd, int raw, char *codepage);

extern void bbs_list(struct user_record *user);

extern void chat_system(struct user_record *user);

extern int mail_getemailcount(struct user_record *user);
extern void send_email(struct user_record *user);
extern void list_emails(struct user_record *user);

extern void download_zmodem(struct user_record *user, char *filename);
extern void settings_menu(struct user_record *user);
extern void upload_zmodem(struct user_record *user, char *upload_p);
extern int ttySetRaw(int fd, struct termios *prevTermios);
extern int do_upload(struct user_record *user, char *final_path);
extern int do_download(struct user_record *user, char *file);
extern void choose_directory(struct user_record *user);
extern void choose_subdir(struct user_record *user);
extern void list_files(struct user_record *user);
extern void upload(struct user_record *user);
extern void download(struct user_record *user);
extern void clear_tagged_files();
extern void next_file_dir(struct user_record *user);
extern void prev_file_dir(struct user_record *user);
extern void next_file_sub(struct user_record *user);
extern void prev_file_sub(struct user_record *user);
extern void file_scan();

extern void lua_push_cfunctions(lua_State *L);
extern void do_lua_script(char *script);

extern void bwave_create_packet();
extern void bwave_upload_reply();

extern void load_strings();
extern char *get_string(int offset);
extern void chomp(char *string);

#if defined(ENABLE_WWW)
extern void www_init();
extern void *www_logger(void * cls, const char * uri, struct MHD_Connection *con);
extern void www_request_completed(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe);
extern int www_handler(void * cls, struct MHD_Connection * connection, const char * url, const char * method, const char * version, const char * upload_data, size_t * upload_data_size, void ** ptr);
extern char *www_email_summary(struct user_record *user);
extern char *www_email_display(struct user_record *user, int email);
extern int www_send_email(struct user_record *user, char *recipient, char *subject, char *body);
extern char *www_new_email();
extern int www_email_delete(struct user_record *user, int id);
extern char *www_msgs_arealist(struct user_record *user);
extern char *www_msgs_messagelist(struct user_record *user, int conference, int area, int skip);
extern char *www_msgs_messageview(struct user_record *user, int conference, int area, int msg);
extern int www_send_msg(struct user_record *user, char *to, char *subj, int conference, int area, char *replyid, char *body);
extern char *www_new_msg(struct user_record *user, int conference, int area);
extern char *www_last10();
#endif
extern int menu_system(char *menufile);
#endif

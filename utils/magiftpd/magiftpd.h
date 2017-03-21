#ifndef __MAGIFTPD_H
#define __MAGIFTPD_H

struct ftpclient {
    int fd;
    int data_socket;
    char current_path[PATH_MAX];
    char data_ip[20];
    int data_port;
    int type;
    char ip[20];
    char name[16];
    char password[32];
    int data_srv_socket;
    int status;
};

struct ftpserver {
    int port;
    char *fileroot;
    char *userdb;
};

#endif

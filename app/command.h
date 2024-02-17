#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>

#include "server.h"
#include "database.h"

#define MAX_CMD_LEN 256
#define MAX_OPER_LEN 32

int read_array_len(char** cmd_ptr);
int read_bulk_string(char** cmd_ptr, char* buff, int max_len);
int write_null(int fd);
int write_simple_string(int fd, char* str, int str_len);
int write_bulk_string(int fd, char* str, int str_len);
int handle_PING(char** cmd_ptr, int *ntokens, int client_fd);
int handle_ECHO(char** cmd_ptr, int *ntokens, int client_fd);
int handle_SET(char** cmd_ptr, int *ntokens, int client_fd, struct database *db);
int handle_GET(char** cmd_ptr, int *ntokens, int client_fd, struct database *db);
int handle_INFO(char** cmd_ptr, int *ntokens, int client_fd, enum server_role role);
int handle_cmd(int client_fd, struct server *svr);

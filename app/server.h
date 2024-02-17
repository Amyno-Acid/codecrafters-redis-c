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

#include "database.h"

#define SERVER_FD_IDX 0
#define MAX_CLIENT 256

enum server_role { master, slave };

struct client_pool {
    int nfds;
    socklen_t client_addr_len;
    struct pollfd fds[MAX_CLIENT];
    struct sockaddr_in client_addrs[MAX_CLIENT];
};

struct server {
    int port, master_port, backlog, fd;
    enum server_role role;
    struct client_pool pool;
    struct database db;
};

int server_init(struct server *this);
int server_close(struct server *this);
void client_pool_init(struct client_pool *this, int server_fd);
int add_client(struct client_pool *this);

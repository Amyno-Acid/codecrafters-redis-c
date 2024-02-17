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

typedef long long ms;
#define DB_SIZE 65536

struct entry {
    bool occupied;
    ms expiry;
    int key_len, val_len;
    char *key, *val;
};

struct database {
    int size, used;
    struct entry *entries;
};

ms mstime();
size_t hash(char* str, int str_len, size_t max);

int db_init(struct database *this, int size);
int db_update(struct database *this, char *key, int key_len, char *val, int val_len, ms expiry);
int db_query(struct database *this, char *key, int key_len, char *val, int val_len);

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>
#include <stdbool.h>

#define SERVER_FD_IDX 0
#define MAX_CLIENT 256
#define MAX_CMD_LEN 256
#define MAX_OPER_LEN 64

struct entry {
    bool occupied;
    int key_len, val_len;
    char *key, *val;
};
struct db {
    int size, used;
    struct entry *entries;
};
int db_init(struct db *this, int size) {
    this->size = size;
    this->used = 0;
    this->entries = calloc(size, sizeof(struct entry));
    if (!this->entries) {
        printf("calloc failed\n");
        return -1;
    }
    return 0;
}
size_t hash(char* str, int str_len, size_t max) {
    size_t ret = 0;
    for (int i = 0; i < str_len; i++)
        ret = (ret << 5) ^ (ret + str[i]);
    return ret % max;
}
int db_insert(struct db *this, char *key, int key_len, char *val, int val_len) {
    if (this->used == this->size) {
        printf("db_insert failed: database is full\n");
        return -1;
    }
    size_t idx = hash(key, key_len, this->size);
    while (this->entries[idx].occupied)
        idx = (idx+1) % this->size;
    this->entries[idx].occupied = 1;
    this->entries[idx].key_len = key_len;
    this->entries[idx].val_len = val_len;
    this->entries[idx].key = malloc(key_len);
    memcpy(this->entries[idx].key, key, key_len);
    this->entries[idx].val = malloc(val_len);
    memcpy(this->entries[idx].val, val, val_len);
    ++this->used;
    return idx;
}
int db_query(struct db *this, char *key, int key_len, char *val, int val_len) {
    size_t idx = hash(key, key_len, this->size);
    size_t stop = idx;
    while (
        !this->entries[idx].occupied ||
        this->entries[idx].key_len != key_len || 
        strncmp(this->entries[idx].key, key, key_len)
    ) {
        idx = (idx+1) % this->size;
        if (idx == stop) {
            printf("Key %.*s not found\n", key_len, key);
            return -1;
        }
    }
    if (val_len > this->entries[idx].val_len)
        val_len = this->entries[idx].val_len;
    memcpy(val, this->entries[idx].val, val_len);
    return val_len;
}

int server_init(int port, int backlog) {
    int server_fd;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return -1;
    }
    // Prevent 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEPORT failed: %s \n", strerror(errno));
        return -1;
    }
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = { htonl(INADDR_ANY) },
    };
    if (bind(server_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return -1;
    }
    if (listen(server_fd, backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return -1;
    }
    return server_fd;
}

struct client_pool {
    struct pollfd fds[MAX_CLIENT];
    struct sockaddr_in client_addrs[MAX_CLIENT];
    int nfds;
    socklen_t client_addr_len;
};
void client_pool_init(struct client_pool *this, int server_fd) {
    this->fds[SERVER_FD_IDX].fd = server_fd;
    this->fds[SERVER_FD_IDX].events = POLLIN;
    this->fds[SERVER_FD_IDX].revents = 0;
    this->nfds = 1;
    this->client_addr_len = sizeof(struct sockaddr_in);
}
int add_client(struct client_pool *this) {
    if (this->nfds == MAX_CLIENT) {
        printf("Max no. clients reached\n");
        return -1;
    }
    int client_fd = accept(
        this->fds[SERVER_FD_IDX].fd,
        (struct sockaddr*) &this->client_addrs[this->nfds],
        &this->client_addr_len
    );
    if (client_fd == -1) {
        printf("Accept failed: %s \n", strerror(errno));
        return -1;
    }
    printf("Client #%d connected\n", this->nfds);
    this->fds[this->nfds].fd = client_fd;
    this->fds[this->nfds].events = POLLIN;
    this->fds[this->nfds].revents = 0;
    return this->nfds++;
}
int read_array_len(char** cmd_ptr) {
    if (**cmd_ptr != '*') {
        printf("Invalid array identifier: %c\n", *cmd_ptr);
        return -1;
    }
    int len = 0;
    while (*(++(*cmd_ptr)) != '\r')
        len = len*10 + (**cmd_ptr-'0');
    *cmd_ptr += 2;
    return len;
}
int read_bulk_string(char** cmd_ptr, char* buff, int max_len) {
    if (**cmd_ptr != '$') {
        printf("Invalid bulk string identifier: %c\n", **cmd_ptr);
        return -1;
    }
    int len = 0;
    while (*(++(*cmd_ptr)) != '\r')
        len = len*10 + (**cmd_ptr-'0');
    *cmd_ptr += 2;
    int i = 0;
    for (; i < len && i < max_len; i++)
        buff[i] = (*cmd_ptr)[i];
    *cmd_ptr += len+2;
    return len;
}
int write_null(int fd) {
    char null[] = "_\r\n";
    int null_len = 3;
    return write(fd, null, null_len);
}
int write_simple_string(int fd, char* str, int str_len) {
    char simple_str[256];
    int simple_str_len = snprintf(simple_str, 256, "+%.*s\r\n", str_len, str);
    return write(fd, simple_str, simple_str_len);
}
int write_bulk_string(int fd, char* str, int str_len) {
    char bulk_str[256];
    if (str_len == -1)
        return write(fd, "$-1\r\n", 5);
    int bulk_str_len = snprintf(bulk_str, 256, "$%d\r\n%.*s\r\n", str_len, str_len, str);
    return write(fd, bulk_str, bulk_str_len);
}
int handle_PING(char** cmd_ptr, int client_fd) {
    write_simple_string(client_fd, "PONG", 4);
    return 0;
}
int handle_ECHO(char** cmd_ptr, int client_fd) {
    char msg[256];
    int msg_len = read_bulk_string(cmd_ptr, msg, 256);
    if (msg_len == -1) {
        printf("ECHO failed\n");
        write_null(client_fd);
        return -1;
    }
    write_bulk_string(client_fd, msg, msg_len);
    return 0;
}
int handle_SET(char** cmd_ptr, int client_fd, struct db *database) {
    char key[256], val[256];
    int key_len = read_bulk_string(cmd_ptr, key, 256);
    int val_len = read_bulk_string(cmd_ptr, val, 256);
    if (key_len == -1 || val_len == -1) {
        printf("SET failed\n");
        write_null(client_fd);
        return -1;
    }
    int idx = db_insert(database, key, key_len, val, val_len);
    if (idx == -1) {
        printf("SET failed\n");
        write_null(client_fd);
        return -1;
    }
    printf("%.*s : %.*s written at %d\n", key_len, key, val_len, val, idx);
    write_simple_string(client_fd, "OK", 2);
    return 0;
}
int handle_GET(char** cmd_ptr, int client_fd, struct db *database) {
    char key[256], val[256];
    int key_len = read_bulk_string(cmd_ptr, key, 256);
    if (key_len == -1) {
        printf("GET failed\n");
        write_null(client_fd);
        return -1;
    }
    int val_len = db_query(database, key, key_len, val, 256);
    write_bulk_string(client_fd, val, val_len);
    return 0;
}
int handle_cmd(int client_fd, struct db *database) {
    char cmd[MAX_CMD_LEN] = {0};
    char* cmd_ptr = cmd;
    if (read(client_fd, cmd, MAX_CMD_LEN-1) == 0) {
        printf("Client exited\n");
        close(client_fd);
        return 1;
    }
    int ntokens = read_array_len(&cmd_ptr);
    char oper[MAX_OPER_LEN];
    int oper_len = read_bulk_string(&cmd_ptr, oper, MAX_OPER_LEN);
    for (int i = 0; i < oper_len; i++)
        oper[i] = toupper(oper[i]);
    if (!strncmp(oper, "PING", oper_len)) {
        handle_PING(&cmd_ptr, client_fd);
    } else if (!strncmp(oper, "ECHO", oper_len)) {
        handle_ECHO(&cmd_ptr, client_fd);
    } else if (!strncmp(oper, "SET", oper_len)) {
        handle_SET(&cmd_ptr, client_fd, database);
    } else if (!strncmp(oper, "GET", oper_len)) {
        handle_GET(&cmd_ptr, client_fd, database);
    } else {
        handle_PING(&cmd_ptr, client_fd);
    }
    return 0;
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);

    int server_fd = server_init(6379, 5);

    struct db database;
    if (db_init(&database, 65536) == -1) {
        printf("db_init failed\n");
        return -1;
    };

    struct client_pool pool;
    client_pool_init(&pool, server_fd);
    
    while (poll(pool.fds, pool.nfds, -1)) {
        for (int i = 0; i < pool.nfds; i++) {
            if (!(pool.fds[i].revents & POLLIN)) 
                continue;
            if (i == SERVER_FD_IDX) {
                add_client(&pool);
            } else {
                handle_cmd(pool.fds[i].fd, &database);
            }
        }
    }

	return 0;
}

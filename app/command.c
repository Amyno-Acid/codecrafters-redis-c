#include "command.h"

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

int handle_PING(char** cmd_ptr, int *ntokens, int client_fd) {
    write_simple_string(client_fd, "PONG", 4);
    *ntokens -= 1;
    printf("Ping\n");
    return 0;
}

int handle_ECHO(char** cmd_ptr, int *ntokens, int client_fd) {
    char msg[256];
    int msg_len = read_bulk_string(cmd_ptr, msg, 256);
    if (msg_len == -1) {
        printf("ECHO failed\n");
        write_null(client_fd);
        return -1;
    }
    write_bulk_string(client_fd, msg, msg_len);
    *ntokens -= 2;
    printf("Echo \"%.*s\"\n", msg_len, msg);
    return 0;
}

int handle_SET(char** cmd_ptr, int *ntokens, int client_fd, struct database *db) {
    char key[256], val[256];
    int key_len = read_bulk_string(cmd_ptr, key, 256);
    int val_len = read_bulk_string(cmd_ptr, val, 256);
    if (key_len == -1 || val_len == -1) {
        printf("SET failed\n");
        write_null(client_fd);
        return -1;
    }
    *ntokens -= 3;

    ms expiry = LLONG_MAX;
    if (*ntokens > 0) {
        char opt[256], arg[256];
        int opt_len = read_bulk_string(cmd_ptr, opt, 256);
        for (int i = 0; i < opt_len; i++)
            opt[i] = toupper(opt[i]);
        if (opt_len == 2 && !strncmp(opt, "PX", 2)) {
            int arg_len = read_bulk_string(cmd_ptr, arg, 255);
            arg[arg_len] = '\0';
            expiry = mstime() + atoll(arg);
        }
    }

    int idx = db_update(db, key, key_len, val, val_len, expiry);
    if (idx == -1) {
        printf("SET failed\n");
        write_null(client_fd);
        return -1;
    }
    printf("Set %.*s -> %.*s (#%d), expiry: %lld\n", key_len, key, val_len, val, idx, expiry);
    write_simple_string(client_fd, "OK", 2);
    return 0;
}

int handle_GET(char** cmd_ptr, int *ntokens, int client_fd, struct database *db) {
    char key[256], val[256];
    int key_len = read_bulk_string(cmd_ptr, key, 256);
    if (key_len == -1) {
        printf("GET %.*s failed\n", key_len, key);
        write_null(client_fd);
        return -1;
    }
    int val_len = db_query(db, key, key_len, val, 256);
    write_bulk_string(client_fd, val, val_len);
    *ntokens -= 2;
    if (val_len != -1)
        printf("Get %.*s -> %.*s at %lld\n", key_len, key, val_len, val, mstime());
    return 0;
}

int handle_INFO(char** cmd_ptr, int *ntokens, int client_fd, enum server_role role) {
    char info[256];
    int info_len = snprintf(info, 256, "role:%s", role == master ? "master" : "slave");
    write_bulk_string(client_fd, info, info_len);
    printf("INFO\n");
    return 0;
}

int handle_cmd(int client_fd, struct server *svr) {
    char cmd[MAX_CMD_LEN] = {0};
    char* cmd_ptr = cmd;
    int cmd_len;
    if ((cmd_len = read(client_fd, cmd, MAX_CMD_LEN-1)) == 0) {
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
        handle_PING(&cmd_ptr, &ntokens, client_fd);
    } else if (!strncmp(oper, "ECHO", oper_len)) {
        handle_ECHO(&cmd_ptr, &ntokens, client_fd);
    } else if (!strncmp(oper, "SET", oper_len)) {
        handle_SET(&cmd_ptr, &ntokens, client_fd, &svr->db);
    } else if (!strncmp(oper, "GET", oper_len)) {
        handle_GET(&cmd_ptr, &ntokens, client_fd, &svr->db);
    } else if (!strncmp(oper, "INFO", oper_len)) {
        handle_INFO(&cmd_ptr, &ntokens, client_fd, svr->role);
    } else {
        handle_PING(&cmd_ptr, &ntokens, client_fd);
    }
    return 0;
}
